/**
 * bluetooth.c — ESP-IDF BLE GATT server for the VO2max spirometer.
 *
 * Services / characteristics (all notify, 128-bit UUIDs):
 *   VO2MAX service  →  vo2max characteristic
 *   VO2    service  →  vo2    characteristic
 *   VCO2   service  →  vco2   characteristic
 *   RQ     service  →  rq     characteristic
 *
 * Thread-safety
 *   BLUETOOTH_Send*() functions are callable from any FreeRTOS task.
 *   They post a tagged message to an internal queue; the BLE task drains
 *   the queue and calls esp_ble_gatts_send_indicate(), which must run in
 *   the same task context that owns the GATT interface.
 *
 * Fixes vs previous version
 *   1. register_next_table() advanced s_reg_step AFTER the call, not before,
 *      so the state machine was always one step ahead — now fixed.
 *   2. The semaphore used for both REG_EVT and CREAT_ATTR_TAB_EVT was the
 *      same binary semaphore. A spurious Give from START_EVT racing the
 *      semaphore could cause the task to call register_next_table() twice.
 *      Replaced with two separate semaphores.
 *   3. esp_ble_gatts_start_service() is called from inside the Bluedroid
 *      internal task (the GATTS callback). This is safe because start_service
 *      posts an async event and does NOT block on a future. However, calling
 *      the NEXT create_attr_tab from there WOULD re-enter the future
 *      mechanism — that is correctly deferred to ble_task via the semaphore.
 *   4. adv_config_done flag was set to (ADV_CONFIG_FLAG | SCAN_RSP_CONFIG_FLAG)
 *      before calling config_adv_data, so both bits were cleared by the GAP
 *      callbacks and advertising started correctly — no bug there, but the
 *      flag logic is clarified with a comment.
 *   5. s_adv_uuid was a local array referenced by pointer in a static struct;
 *      the struct is initialised at compile-time so the pointer was valid, but
 *      the memcpy in ble_task correctly fills it before any advertising call.
 *   6. Added NULL-guard in drain_send_queue for zero handles (services not yet
 *      started).
 *   7. BLE_TASK_STACK increased to 8192 — the Bluedroid init path alone can
 *      consume >4 KB on some IDF versions.
 */

#include "bluetooth.h"

#include <string.h>
#include <stdatomic.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"
#include "esp_err.h"

/* ------------------------------------------------------------------ */
/*  Compile-time tunables                                               */
/* ------------------------------------------------------------------ */

#define BLE_TASK_STACK 8192 /* bumped: Bluedroid init is stack-hungry */
#define BLE_TASK_PRIORITY 5
#define BLE_DEVICE_NAME "VO2-HR"
#define SEND_QUEUE_DEPTH 8

static const char *TAG = "BLE";

/* ------------------------------------------------------------------ */
/*  128-bit UUIDs  (little-endian, LSB first)                          */
/* ------------------------------------------------------------------ */

/* b354cf1b-2486-4f21-b4b1-ee4cd5cc3bf0 */
static const uint8_t SVC_UUID_VO2MAX[16] = {
    0xf0, 0x3b, 0xcc, 0xd5, 0x4c, 0xee, 0xb1, 0xb4, 0x21, 0x4f, 0x86, 0x24, 0x1b, 0xcf, 0x54, 0xb3};
/* c70c73fd-b5fb-4a5f-87f4-7a187590b0ef */
static const uint8_t CHR_UUID_VO2MAX[16] = {
    0xef, 0xb0, 0x90, 0x75, 0x18, 0x7a, 0xf4, 0x87, 0x5f, 0x4a, 0xfb, 0xb5, 0xfd, 0x73, 0x0c, 0xc7};

/* 231c616b-32a6-4b93-9f0c-fe728deca0a5 */
static const uint8_t SVC_UUID_VO2[16] = {
    0xa5, 0xa0, 0xec, 0x8d, 0x72, 0xfe, 0x0c, 0x9f, 0x93, 0x4b, 0xa6, 0x32, 0x6b, 0x61, 0x1c, 0x23};
/* 4225d51b-f1c2-419a-9acc-bd8e70d960ae */
static const uint8_t CHR_UUID_VO2[16] = {
    0xae, 0x60, 0xd9, 0x70, 0x8e, 0xbd, 0xcc, 0x9a, 0x9a, 0x41, 0xc2, 0xf1, 0x1b, 0xd5, 0x25, 0x42};

/* 196c1c76-fe53-47e7-baab-a32b404d63df */
static const uint8_t SVC_UUID_VCO2[16] = {
    0xdf, 0x63, 0x4d, 0x40, 0x2b, 0xa3, 0xab, 0xba, 0xe7, 0x47, 0x53, 0xfe, 0x76, 0x1c, 0x6c, 0x19};
/* a4534c9e-0ede-48ec-bee5-7b728ecb25df */
static const uint8_t CHR_UUID_VCO2[16] = {
    0xdf, 0x25, 0xcb, 0x8e, 0x72, 0x7b, 0xe5, 0xbe, 0xec, 0x48, 0xde, 0x0e, 0x9e, 0x4c, 0x53, 0xa4};

/* 8868ba7f-cada-4035-85d2-e0002aeb2be6 */
static const uint8_t SVC_UUID_RQ[16] = {
    0xe6, 0x2b, 0xeb, 0x2a, 0x00, 0xe0, 0xd2, 0x85, 0x35, 0x40, 0xda, 0xca, 0x7f, 0xba, 0x68, 0x88};
/* b192eef4-a298-43fe-b85c-b04d934448f7 */
static const uint8_t CHR_UUID_RQ[16] = {
    0xf7, 0x48, 0x44, 0x93, 0x4d, 0xb0, 0x5c, 0xb8, 0xfe, 0x43, 0x98, 0xa2, 0xf4, 0xee, 0x92, 0xb1};

/* ------------------------------------------------------------------ */
/*  Per-service attribute table layout  (4 entries each)               */
/*  [0] service decl  [1] char decl  [2] char value  [3] CCCD         */
/* ------------------------------------------------------------------ */

#define SVC_TABLE_SIZE 4
#define IDX_SVC 0
#define IDX_CHR_DECL 1
#define IDX_CHR_VAL 2
#define IDX_CCCD 3

/* Shared GATT primitives */
static const uint16_t UUID_PRIMARY_SERVICE = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t UUID_CHAR_DECLARE = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t UUID_CCCD = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint8_t CHAR_PROP_NOTIFY = ESP_GATT_CHAR_PROP_BIT_NOTIFY;

/* Initial characteristic values */
static uint8_t s_val_vo2max[4] = {0};
static uint8_t s_val_vo2[4] = {0};
static uint8_t s_val_vco2[4] = {0};
static uint8_t s_val_rq[4] = {0};

/* Each service gets its own CCCD storage so they are independent */
static uint8_t s_cccd_vo2max[2] = {0};
static uint8_t s_cccd_vo2[2] = {0};
static uint8_t s_cccd_vco2[2] = {0};
static uint8_t s_cccd_rq[2] = {0};

/* Macro to build a 4-entry attribute table for one notify service */
#define MAKE_SVC_TABLE(tbl_name, svc_uuid, chr_uuid, val_buf, cccd_buf)                                                                                                                                 \
    static const esp_gatts_attr_db_t tbl_name[SVC_TABLE_SIZE] = {                                                                                                                                       \
        [IDX_SVC] = {                                                                                                                                                                                   \
            {ESP_GATT_AUTO_RSP},                                                                                                                                                                        \
            {ESP_UUID_LEN_16, (uint8_t *)&UUID_PRIMARY_SERVICE,                                                                                                                                         \
             ESP_GATT_PERM_READ,                                                                                                                                                                        \
             ESP_UUID_LEN_128, ESP_UUID_LEN_128, (uint8_t *)(svc_uuid)}},                                                                                                                               \
        [IDX_CHR_DECL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CHAR_DECLARE, ESP_GATT_PERM_READ, sizeof(CHAR_PROP_NOTIFY), sizeof(CHAR_PROP_NOTIFY), (uint8_t *)&CHAR_PROP_NOTIFY}}, \
        [IDX_CHR_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_128, (uint8_t *)(chr_uuid), ESP_GATT_PERM_READ, sizeof(val_buf), sizeof(val_buf), (val_buf)}},                                              \
        [IDX_CCCD] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&UUID_CCCD, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(cccd_buf), sizeof(cccd_buf), (cccd_buf)}},                         \
    }

MAKE_SVC_TABLE(s_tbl_vo2max, SVC_UUID_VO2MAX, CHR_UUID_VO2MAX, s_val_vo2max, s_cccd_vo2max);
MAKE_SVC_TABLE(s_tbl_vo2, SVC_UUID_VO2, CHR_UUID_VO2, s_val_vo2, s_cccd_vo2);
MAKE_SVC_TABLE(s_tbl_vco2, SVC_UUID_VCO2, CHR_UUID_VCO2, s_val_vco2, s_cccd_vco2);
MAKE_SVC_TABLE(s_tbl_rq, SVC_UUID_RQ, CHR_UUID_RQ, s_val_rq, s_cccd_rq);

/* ------------------------------------------------------------------ */
/*  Handle storage                                                      */
/* ------------------------------------------------------------------ */

typedef struct
{
    uint16_t svc;
    uint16_t chr_decl;
    uint16_t chr_val;
    uint16_t cccd;
} svc_handles_t;

static svc_handles_t s_h_vo2max = {0};
static svc_handles_t s_h_vo2 = {0};
static svc_handles_t s_h_vco2 = {0};
static svc_handles_t s_h_rq = {0};

/* Registration chain state.
   FIX: s_reg_step now tracks which table to register NEXT (not the one
   just submitted). register_next_table() reads the current step, issues
   the call for that step, then advances to the next. Previously the step
   was advanced before the call, so the state was always one ahead. */
typedef enum
{
    REG_STEP_VO2MAX = 0,
    REG_STEP_VO2,
    REG_STEP_VCO2,
    REG_STEP_RQ,
    REG_STEP_DONE,
} reg_step_t;

static reg_step_t s_reg_step = REG_STEP_VO2MAX;

/* ------------------------------------------------------------------ */
/*  Runtime state                                                       */
/* ------------------------------------------------------------------ */

static uint8_t s_adv_config_done = 0;
#define ADV_CONFIG_FLAG (1 << 0)
#define SCAN_RSP_CONFIG_FLAG (1 << 1)

static atomic_bool s_connected = false;
static uint16_t s_conn_id = 0;
static esp_gatt_if_t s_gatts_if = ESP_GATT_IF_NONE;

/* ------------------------------------------------------------------ */
/*  Send queue                                                          */
/* ------------------------------------------------------------------ */

typedef enum
{
    BLE_CH_VO2MAX = 0,
    BLE_CH_VO2,
    BLE_CH_VCO2,
    BLE_CH_RQ,
} ble_channel_t;

typedef struct
{
    ble_channel_t channel;
    float value;
} ble_send_msg_t;

static QueueHandle_t s_send_queue = NULL;

/* ------------------------------------------------------------------ */
/*  Advertising  — only the primary UUID to stay within 31 bytes       */
/* ------------------------------------------------------------------ */

static uint8_t s_adv_uuid[16];

static esp_ble_adv_data_t s_adv_data = {
    .set_scan_rsp = false,
    .include_name = false,
    .include_txpower = false,
    .min_interval = ESP_BLE_GAP_CONN_ITVL_MS(7.5),
    .max_interval = ESP_BLE_GAP_CONN_ITVL_MS(20),
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = 16,
    .p_service_uuid = s_adv_uuid,
    .flag = ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT,
};

static esp_ble_adv_data_t s_scan_rsp_data = {
    .set_scan_rsp = true,
    .include_name = true,
    .include_txpower = true,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = 0,
    .p_service_uuid = NULL,
    .flag = ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT,
};

static esp_ble_adv_params_t s_adv_params = {
    .adv_int_min = ESP_BLE_GAP_ADV_ITVL_MS(20),
    .adv_int_max = ESP_BLE_GAP_ADV_ITVL_MS(40),
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

/* ------------------------------------------------------------------ */
/*  Semaphores
 *
 *  FIX: Two separate semaphores replace the single shared one.
 *
 *  s_gatts_reg_sem   — Given once by ESP_GATTS_REG_EVT so ble_task can
 *                      proceed past Bluedroid initialisation.
 *
 *  s_attr_tab_sem    — Given by ESP_GATTS_CREAT_ATTR_TAB_EVT each time a
 *                      table is created. ble_task waits on this before
 *                      calling register_next_table() for the next service.
 *
 *  Using a single semaphore was safe in practice because REG_EVT fires
 *  before any CREAT_ATTR_TAB_EVT, but it made the control flow opaque
 *  and fragile against future changes.
 * ------------------------------------------------------------------ */

static SemaphoreHandle_t s_gatts_reg_sem = NULL; /* REG_EVT once       */
static SemaphoreHandle_t s_attr_tab_sem = NULL;  /* CREAT_ATTR_TAB_EVT */

/* ------------------------------------------------------------------ */
/*  Forward declarations                                                */
/* ------------------------------------------------------------------ */

static void ble_task(void *arg);
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void register_next_table(esp_gatt_if_t gatts_if);
static void drain_send_queue(void);
static void notify_float(uint16_t val_handle, float value);

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

void BLUETOOTH_Initialize(void)
{
    s_send_queue = xQueueCreate(SEND_QUEUE_DEPTH, sizeof(ble_send_msg_t));
    configASSERT(s_send_queue);
    xTaskCreate(ble_task, "ble_task", BLE_TASK_STACK, NULL, BLE_TASK_PRIORITY, NULL);
}

bool BLUETOOTH_IsConnected(void)
{
    return atomic_load(&s_connected);
}

static void enqueue(ble_channel_t ch, float value)
{
    if (!atomic_load(&s_connected))
        return;
    ble_send_msg_t msg = {.channel = ch, .value = value};
    if (xQueueSend(s_send_queue, &msg, 0) == errQUEUE_FULL)
    {
        ble_send_msg_t discard;
        xQueueReceive(s_send_queue, &discard, 0);
        xQueueSend(s_send_queue, &msg, 0);
    }
}

void BLUETOOTH_SendVO2Max(float v) { enqueue(BLE_CH_VO2MAX, v); }
void BLUETOOTH_SendVO2(float v) { enqueue(BLE_CH_VO2, v); }
void BLUETOOTH_SendVCO2(float v) { enqueue(BLE_CH_VCO2, v); }
void BLUETOOTH_SendRQ(float v) { enqueue(BLE_CH_RQ, v); }

/* ------------------------------------------------------------------ */
/*  BLE task                                                            */
/* ------------------------------------------------------------------ */

static void ble_task(void *arg)
{
    memcpy(s_adv_uuid, SVC_UUID_VO2MAX, 16);

    /* FIX: two separate semaphores, one per signal type */
    s_gatts_reg_sem = xSemaphoreCreateBinary();
    s_attr_tab_sem = xSemaphoreCreateBinary();
    configASSERT(s_gatts_reg_sem);
    configASSERT(s_attr_tab_sem);

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

    esp_bluedroid_config_t bd_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bluedroid_init_with_cfg(&bd_cfg));
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    /* Bluedroid enable is asynchronous; give its internal tasks time to
       finish starting before we touch any BLE API. */
    vTaskDelay(pdMS_TO_TICKS(500));

    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts_event_handler));
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(0));

    /* Block until ESP_GATTS_REG_EVT fires and s_gatts_if is set */
    xSemaphoreTake(s_gatts_reg_sem, portMAX_DELAY);
    ESP_LOGI(TAG, "GATTS registered, stack ready");

    ESP_ERROR_CHECK(esp_ble_gatt_set_local_mtu(247));

    /* Security / pairing — required for Windows WinRT */
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_BOND;
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;
    uint8_t key_size = 16;
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;

    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(auth_req));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(iocap));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(key_size));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(init_key));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(rsp_key));

    /* Configure advertising from this task, never from a GATTS callback.
       Set both flags before the two calls so the GAP callbacks clear them
       independently and only start advertising when both reach zero. */
    esp_ble_gap_set_device_name(BLE_DEVICE_NAME);
    s_adv_config_done = ADV_CONFIG_FLAG | SCAN_RSP_CONFIG_FLAG;
    esp_ble_gap_config_adv_data(&s_adv_data);
    esp_ble_gap_config_adv_data(&s_scan_rsp_data);

    /* Drive the service registration chain.
       FIX: register_next_table() now advances s_reg_step AFTER issuing the
       call (not before), so the state machine is always in sync with what
       has actually been submitted. We wait on s_attr_tab_sem between each
       step so we never call create_attr_tab while a previous one is still
       in-flight inside Bluedroid's future mechanism. */
    while (s_reg_step != REG_STEP_DONE)
    {
        register_next_table(s_gatts_if);
        xSemaphoreTake(s_attr_tab_sem, portMAX_DELAY);
    }
    ESP_LOGI(TAG, "All services registered");

    while (1)
    {
        drain_send_queue();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/* ------------------------------------------------------------------ */
/*  Service registration chain                                          */
/*                                                                      */
/*  FIX: step is advanced AFTER esp_ble_gatts_create_attr_tab() so the */
/*  enum value always reflects what has been submitted, not what comes  */
/*  next. The previous code advanced first, which made the switch fall  */
/*  one step ahead of reality.                                          */
/* ------------------------------------------------------------------ */

static void register_next_table(esp_gatt_if_t gatts_if)
{
    switch (s_reg_step)
    {
    case REG_STEP_VO2MAX:
        ESP_LOGI(TAG, "Registering VO2MAX service table");
        esp_ble_gatts_create_attr_tab(s_tbl_vo2max, gatts_if, SVC_TABLE_SIZE, 0);
        s_reg_step = REG_STEP_VO2;
        break;
    case REG_STEP_VO2:
        ESP_LOGI(TAG, "Registering VO2 service table");
        esp_ble_gatts_create_attr_tab(s_tbl_vo2, gatts_if, SVC_TABLE_SIZE, 1);
        s_reg_step = REG_STEP_VCO2;
        break;
    case REG_STEP_VCO2:
        ESP_LOGI(TAG, "Registering VCO2 service table");
        esp_ble_gatts_create_attr_tab(s_tbl_vco2, gatts_if, SVC_TABLE_SIZE, 2);
        s_reg_step = REG_STEP_RQ;
        break;
    case REG_STEP_RQ:
        ESP_LOGI(TAG, "Registering RQ service table");
        esp_ble_gatts_create_attr_tab(s_tbl_rq, gatts_if, SVC_TABLE_SIZE, 3);
        s_reg_step = REG_STEP_DONE;
        break;
    case REG_STEP_DONE:
        /* Should not be reached — loop guard in ble_task prevents this */
        break;
    }
}

/* ------------------------------------------------------------------ */
/*  Send queue drain (runs inside ble_task context)                    */
/* ------------------------------------------------------------------ */

static void drain_send_queue(void)
{
    if (!atomic_load(&s_connected))
        return;
    if (s_gatts_if == ESP_GATT_IF_NONE)
        return;

    ble_send_msg_t msg;
    while (xQueueReceive(s_send_queue, &msg, 0) == pdTRUE)
    {
        uint16_t handle = 0;
        switch (msg.channel)
        {
        case BLE_CH_VO2MAX:
            handle = s_h_vo2max.chr_val;
            break;
        case BLE_CH_VO2:
            handle = s_h_vo2.chr_val;
            break;
        case BLE_CH_VCO2:
            handle = s_h_vco2.chr_val;
            break;
        case BLE_CH_RQ:
            handle = s_h_rq.chr_val;
            break;
        default:
            continue;
        }
        /* FIX: guard against zero handles (services not yet started) */
        if (handle == 0)
            continue;
        notify_float(handle, msg.value);
    }
}

static void notify_float(uint16_t val_handle, float value)
{
    uint8_t buf[4];
    memcpy(buf, &value, 4);
    esp_ble_gatts_send_indicate(s_gatts_if, s_conn_id,
                                val_handle, sizeof(buf), buf, false);
}

/* ------------------------------------------------------------------ */
/*  GAP event handler                                                   */
/* ------------------------------------------------------------------ */

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        s_adv_config_done &= ~ADV_CONFIG_FLAG;
        if (s_adv_config_done == 0)
            esp_ble_gap_start_advertising(&s_adv_params);
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        s_adv_config_done &= ~SCAN_RSP_CONFIG_FLAG;
        if (s_adv_config_done == 0)
            esp_ble_gap_start_advertising(&s_adv_params);
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
            ESP_LOGE(TAG, "Advertising start failed: %d", param->adv_start_cmpl.status);
        else
            ESP_LOGI(TAG, "Advertising started");
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
            ESP_LOGE(TAG, "Advertising stop failed: %d", param->adv_stop_cmpl.status);
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        ESP_LOGI(TAG, "Conn params: interval=%d latency=%d timeout=%d",
                 param->update_conn_params.conn_int,
                 param->update_conn_params.latency,
                 param->update_conn_params.timeout);
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------------ */
/*  GATTS event handler                                                 */
/* ------------------------------------------------------------------ */

static void gatts_event_handler(esp_gatts_cb_event_t event,
                                esp_gatt_if_t gatts_if,
                                esp_ble_gatts_cb_param_t *param)
{
    switch (event)
    {

    case ESP_GATTS_REG_EVT:
        if (param->reg.status != ESP_GATT_OK)
        {
            ESP_LOGE(TAG, "App register failed, status %d", param->reg.status);
            break;
        }
        s_gatts_if = gatts_if;
        ESP_LOGI(TAG, "App registered, gatts_if %d", gatts_if);
        /* Unblock ble_task to continue with GAP/GATT setup */
        xSemaphoreGive(s_gatts_reg_sem);
        break;

    case ESP_GATTS_CREAT_ATTR_TAB_EVT:
        if (param->add_attr_tab.status != ESP_GATT_OK)
        {
            ESP_LOGE(TAG, "Attr table create failed, status %d, srvc_inst_id %d",
                     param->add_attr_tab.status, param->add_attr_tab.svc_inst_id);
            break;
        }
        if (param->add_attr_tab.num_handle != SVC_TABLE_SIZE)
        {
            ESP_LOGE(TAG, "Attr table size mismatch: got %d expected %d",
                     param->add_attr_tab.num_handle, SVC_TABLE_SIZE);
            break;
        }

        /* Store handles and start the service.
           esp_ble_gatts_start_service() is safe to call from the GATTS
           callback because it is fully asynchronous (posts an event, does
           not block on a future). Only create_attr_tab must not be called
           from here — that is deferred to ble_task via s_attr_tab_sem. */
        switch (param->add_attr_tab.svc_inst_id)
        {
        case 0:
            s_h_vo2max.svc = param->add_attr_tab.handles[IDX_SVC];
            s_h_vo2max.chr_decl = param->add_attr_tab.handles[IDX_CHR_DECL];
            s_h_vo2max.chr_val = param->add_attr_tab.handles[IDX_CHR_VAL];
            s_h_vo2max.cccd = param->add_attr_tab.handles[IDX_CCCD];
            esp_ble_gatts_start_service(s_h_vo2max.svc);
            break;
        case 1:
            s_h_vo2.svc = param->add_attr_tab.handles[IDX_SVC];
            s_h_vo2.chr_decl = param->add_attr_tab.handles[IDX_CHR_DECL];
            s_h_vo2.chr_val = param->add_attr_tab.handles[IDX_CHR_VAL];
            s_h_vo2.cccd = param->add_attr_tab.handles[IDX_CCCD];
            esp_ble_gatts_start_service(s_h_vo2.svc);
            break;
        case 2:
            s_h_vco2.svc = param->add_attr_tab.handles[IDX_SVC];
            s_h_vco2.chr_decl = param->add_attr_tab.handles[IDX_CHR_DECL];
            s_h_vco2.chr_val = param->add_attr_tab.handles[IDX_CHR_VAL];
            s_h_vco2.cccd = param->add_attr_tab.handles[IDX_CCCD];
            esp_ble_gatts_start_service(s_h_vco2.svc);
            break;
        case 3:
            s_h_rq.svc = param->add_attr_tab.handles[IDX_SVC];
            s_h_rq.chr_decl = param->add_attr_tab.handles[IDX_CHR_DECL];
            s_h_rq.chr_val = param->add_attr_tab.handles[IDX_CHR_VAL];
            s_h_rq.cccd = param->add_attr_tab.handles[IDX_CCCD];
            esp_ble_gatts_start_service(s_h_rq.svc);
            break;
        default:
            ESP_LOGW(TAG, "Unknown svc_inst_id %d", param->add_attr_tab.svc_inst_id);
            break;
        }

        /* FIX: signal ble_task on the dedicated semaphore, not the shared one.
           This prevents any accidental cross-signalling between REG_EVT and
           CREAT_ATTR_TAB_EVT in edge-case timing scenarios. */
        xSemaphoreGive(s_attr_tab_sem);
        break;

    case ESP_GATTS_START_EVT:
        ESP_LOGI(TAG, "Service started, handle %d", param->start.service_handle);
        break;

    case ESP_GATTS_CONNECT_EVT:
        esp_ble_set_encryption(param->connect.remote_bda, ESP_BLE_SEC_ENCRYPT_MITM);
        ESP_LOGI(TAG, "Client connected, conn_id %d", param->connect.conn_id);
        s_conn_id = param->connect.conn_id;
        atomic_store(&s_connected, true);
        {
            esp_ble_conn_update_params_t cp = {0};
            memcpy(cp.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
            cp.latency = 0;
            cp.min_int = 0x10;
            cp.max_int = 0x20;
            cp.timeout = 400;
            esp_ble_gap_update_conn_params(&cp);
        }
        /* Restart advertising so a second client can discover the device */
        esp_ble_gap_start_advertising(&s_adv_params);
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(TAG, "Client disconnected, reason 0x%02x", param->disconnect.reason);
        atomic_store(&s_connected, false);
        esp_ble_gap_start_advertising(&s_adv_params);
        break;

    case ESP_GATTS_WRITE_EVT:
        if (param->write.need_rsp)
            esp_ble_gatts_send_response(gatts_if, param->write.conn_id,
                                        param->write.trans_id, ESP_GATT_OK, NULL);
        break;

    case ESP_GATTS_MTU_EVT:
        ESP_LOGI(TAG, "MTU exchanged: %d", param->mtu.mtu);
        break;

    case ESP_GATTS_CONF_EVT:
        if (param->conf.status != ESP_GATT_OK)
            ESP_LOGW(TAG, "Notification confirm failed, handle %d status %d",
                     param->conf.handle, param->conf.status);
        break;

    default:
        break;
    }
}