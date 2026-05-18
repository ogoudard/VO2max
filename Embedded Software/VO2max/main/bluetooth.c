/**
 * bluetooth_bridge.c — BLE bridge + spirometer server for LilyGo T-Display S3
 *
 * See bluetooth_bridge.h for the full architecture description.
 *
 * ── GATTS (peripheral / server) ─────────────────────────────────────
 *  6 services registered sequentially via the semaphore-gated chain:
 *    inst 0  Heart Rate        0x180D / 0x2A37   (relay from sensor)
 *    inst 1  Cycling Power     0x1818 / 0x2A63   (relay from sensor)
 *    inst 2  VO2Max            custom 128-bit UUIDs
 *    inst 3  VO2               custom 128-bit UUIDs
 *    inst 4  VCO2              custom 128-bit UUIDs
 *    inst 5  RQ                custom 128-bit UUIDs
 *
 * ── GATTC (central / client) ────────────────────────────────────────
 *  Two app registrations (GATTC_APP_HR=0, GATTC_APP_POWER=1).
 *  GAP scan parser identifies HRS/CPS by 16-bit UUID in adv payload
 *  AND scan response payload.
 *  After discovery the CCCD is written to enable notifications.
 *  On disconnect the state resets and scanning resumes automatically.
 *
 * ── Send queue (thread-safe outbound path) ──────────────────────────
 *  BLUETOOTH_Send*() post tagged float messages to s_send_queue.
 *  The bridge task drains the queue every DRAIN_PERIOD_MS and calls
 *  esp_ble_gatts_send_indicate() from its own context, which satisfies
 *  Bluedroid's requirement that send_indicate not be called from an
 *  arbitrary task.
 *
 * ── Relay path (sensor → smartwatch) ────────────────────────────────
 *  relay_hr() / relay_power() are called directly from the GATTC
 *  notify callback (already inside the Bluedroid task) so they call
 *  esp_ble_gatts_send_indicate() directly — no queue needed.
 *
 * Key fixes:
 *   • Two separate semaphores (reg vs attr_tab) — no cross-signalling.
 *   • register_next_server_table() advances state AFTER the API call.
 *   • Zero-handle guard before every send_indicate call.
 *   • Stack size 8192 for Bluedroid init headroom.
 *   • No security params set — ESP_GAP_BLE_SEC_REQ_EVT accepted freely.
 *   • Char + CCCD lookup moved to SEARCH_CMPL_EVT (cache valid then).
 *   • Scan response bytes parsed from ble_adv + adv_data_len offset.
 *   • [FIX] adv config triggered after device name set with short delay
 *     instead of ESP_GAP_BLE_LOCAL_NAME_SET_COMPLETE_EVT (not available
 *     in all IDF v6 builds) — eliminates 0x102 (ESP_ERR_INVALID_STATE).
 *   • [FIX] s_adv_config_done flags set on completion (not pre-cleared)
 *     so advertising only starts once BOTH configs are confirmed.
 *   • [FIX] Scanning starts after service registration, not gated on
 *     downstream connection, so sensors can be found independently.
 *   • [FIX] esp_ble_gattc_register_for_notify() called before writing
 *     the CCCD so Bluedroid actually routes ESP_GATTC_NOTIFY_EVT to the
 *     callback. Without this call notifications are silently discarded.
 */

#include "bluetooth.h"

#include <string.h>
#include <stdatomic.h>
#include <stdbool.h>

#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"
#include "esp_err.h"

#include "plot.h"
#include "esp_timer.h"

static const char *TAG = "[BLUETOOTH]";

/* ================================================================== */
/*  Compile-time tunables                                               */
/* ================================================================== */

#define BRIDGE_TASK_STACK 8192
#define BRIDGE_TASK_PRIORITY 5
#define BLE_DEVICE_NAME "LilyGo-Bridge"
#define SEND_QUEUE_DEPTH 16
#define DRAIN_PERIOD_MS 50

/* ================================================================== */
/*  Well-known 16-bit UUIDs                                            */
/* ================================================================== */

#define UUID16_HRS 0x180D
#define UUID16_HR_MEAS 0x2A37
#define UUID16_CPS 0x1818
#define UUID16_CP_MEAS 0x2A63
#define UUID16_CCCD 0x2902

/* ================================================================== */
/*  Custom 128-bit UUIDs for spirometer services (little-endian)       */
/* ================================================================== */

/* VO2Max service  b354cf1b-2486-4f21-b4b1-ee4cd5cc3bf0 */
static const uint8_t SVC_UUID_VO2MAX[16] = {
    0xf0, 0x3b, 0xcc, 0xd5, 0x4c, 0xee, 0xb1, 0xb4, 0x21, 0x4f, 0x86, 0x24, 0x1b, 0xcf, 0x54, 0xb3};
/* VO2Max char     c70c73fd-b5fb-4a5f-87f4-7a187590b0ef */
static const uint8_t CHR_UUID_VO2MAX[16] = {
    0xef, 0xb0, 0x90, 0x75, 0x18, 0x7a, 0xf4, 0x87, 0x5f, 0x4a, 0xfb, 0xb5, 0xfd, 0x73, 0x0c, 0xc7};

/* VO2 service     231c616b-32a6-4b93-9f0c-fe728deca0a5 */
static const uint8_t SVC_UUID_VO2[16] = {
    0xa5, 0xa0, 0xec, 0x8d, 0x72, 0xfe, 0x0c, 0x9f, 0x93, 0x4b, 0xa6, 0x32, 0x6b, 0x61, 0x1c, 0x23};
/* VO2 char        4225d51b-f1c2-419a-9acc-bd8e70d960ae */
static const uint8_t CHR_UUID_VO2[16] = {
    0xae, 0x60, 0xd9, 0x70, 0x8e, 0xbd, 0xcc, 0x9a, 0x9a, 0x41, 0xc2, 0xf1, 0x1b, 0xd5, 0x25, 0x42};

/* VCO2 service    196c1c76-fe53-47e7-baab-a32b404d63df */
static const uint8_t SVC_UUID_VCO2[16] = {
    0xdf, 0x63, 0x4d, 0x40, 0x2b, 0xa3, 0xab, 0xba, 0xe7, 0x47, 0x53, 0xfe, 0x76, 0x1c, 0x6c, 0x19};
/* VCO2 char       a4534c9e-0ede-48ec-bee5-7b728ecb25df */
static const uint8_t CHR_UUID_VCO2[16] = {
    0xdf, 0x25, 0xcb, 0x8e, 0x72, 0x7b, 0xe5, 0xbe, 0xec, 0x48, 0xde, 0x0e, 0x9e, 0x4c, 0x53, 0xa4};

/* RQ service      8868ba7f-cada-4035-85d2-e0002aeb2be6 */
static const uint8_t SVC_UUID_RQ[16] = {
    0xe6, 0x2b, 0xeb, 0x2a, 0x00, 0xe0, 0xd2, 0x85, 0x35, 0x40, 0xda, 0xca, 0x7f, 0xba, 0x68, 0x88};
/* RQ char         b192eef4-a298-43fe-b85c-b04d934448f7 */
static const uint8_t CHR_UUID_RQ[16] = {
    0xf7, 0x48, 0x44, 0x93, 0x4d, 0xb0, 0x5c, 0xb8, 0xfe, 0x43, 0x98, 0xa2, 0xf4, 0xee, 0x92, 0xb1};

/* ================================================================== */
/*  GATTS attribute table helpers                                       */
/* ================================================================== */

#define SVC_TABLE_SIZE 4
#define IDX_SVC 0
#define IDX_CHR_DECL 1
#define IDX_CHR_VAL 2
#define IDX_CCCD 3

static const uint16_t s_uuid_primary = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t s_uuid_char_decl = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t s_uuid_cccd = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

static const uint16_t s_uuid16_hrs = UUID16_HRS;
static const uint16_t s_uuid16_hr_meas = UUID16_HR_MEAS;
static const uint16_t s_uuid16_cps = UUID16_CPS;
static const uint16_t s_uuid16_cp_meas = UUID16_CP_MEAS;

static const uint8_t s_prop_notify = ESP_GATT_CHAR_PROP_BIT_NOTIFY;

/* ---- Value + CCCD buffers ---- */
static uint8_t s_val_hr[4] = {0};
static uint8_t s_val_power[8] = {0};
static uint8_t s_val_vo2max[4] = {0};
static uint8_t s_val_vo2[4] = {0};
static uint8_t s_val_vco2[4] = {0};
static uint8_t s_val_rq[4] = {0};

static uint8_t s_cccd_hr[2] = {0};
static uint8_t s_cccd_power[2] = {0};
static uint8_t s_cccd_vo2max[2] = {0};
static uint8_t s_cccd_vo2[2] = {0};
static uint8_t s_cccd_vco2[2] = {0};
static uint8_t s_cccd_rq[2] = {0};

/* ---- Attribute tables ---- */

/* HR (16-bit UUIDs) */
static const esp_gatts_attr_db_t s_tbl_hr[SVC_TABLE_SIZE] = {
    [IDX_SVC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&s_uuid_primary, ESP_GATT_PERM_READ, ESP_UUID_LEN_16, ESP_UUID_LEN_16, (uint8_t *)&s_uuid16_hrs}},
    [IDX_CHR_DECL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&s_uuid_char_decl, ESP_GATT_PERM_READ, sizeof(s_prop_notify), sizeof(s_prop_notify), (uint8_t *)&s_prop_notify}},
    [IDX_CHR_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&s_uuid16_hr_meas, ESP_GATT_PERM_READ, sizeof(s_val_hr), sizeof(s_val_hr), s_val_hr}},
    [IDX_CCCD] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&s_uuid_cccd, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(s_cccd_hr), sizeof(s_cccd_hr), s_cccd_hr}},
};

/* Power (16-bit UUIDs) */
static const esp_gatts_attr_db_t s_tbl_power[SVC_TABLE_SIZE] = {
    [IDX_SVC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&s_uuid_primary, ESP_GATT_PERM_READ, ESP_UUID_LEN_16, ESP_UUID_LEN_16, (uint8_t *)&s_uuid16_cps}},
    [IDX_CHR_DECL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&s_uuid_char_decl, ESP_GATT_PERM_READ, sizeof(s_prop_notify), sizeof(s_prop_notify), (uint8_t *)&s_prop_notify}},
    [IDX_CHR_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&s_uuid16_cp_meas, ESP_GATT_PERM_READ, sizeof(s_val_power), sizeof(s_val_power), s_val_power}},
    [IDX_CCCD] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&s_uuid_cccd, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(s_cccd_power), sizeof(s_cccd_power), s_cccd_power}},
};

/* Macro for the 4 custom 128-bit spirometer services */
#define MAKE_SVC_TABLE(tbl, svc_uuid, chr_uuid, val_buf, cccd_buf)                                                                                                                            \
    static const esp_gatts_attr_db_t tbl[SVC_TABLE_SIZE] = {                                                                                                                                  \
        [IDX_SVC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&s_uuid_primary, ESP_GATT_PERM_READ, ESP_UUID_LEN_128, ESP_UUID_LEN_128, (uint8_t *)(svc_uuid)}},                      \
        [IDX_CHR_DECL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&s_uuid_char_decl, ESP_GATT_PERM_READ, sizeof(s_prop_notify), sizeof(s_prop_notify), (uint8_t *)&s_prop_notify}}, \
        [IDX_CHR_VAL] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_128, (uint8_t *)(chr_uuid), ESP_GATT_PERM_READ, sizeof(val_buf), sizeof(val_buf), (val_buf)}},                                    \
        [IDX_CCCD] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&s_uuid_cccd, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(cccd_buf), sizeof(cccd_buf), (cccd_buf)}},             \
    }

MAKE_SVC_TABLE(s_tbl_vo2max, SVC_UUID_VO2MAX, CHR_UUID_VO2MAX, s_val_vo2max, s_cccd_vo2max);
MAKE_SVC_TABLE(s_tbl_vo2, SVC_UUID_VO2, CHR_UUID_VO2, s_val_vo2, s_cccd_vo2);
MAKE_SVC_TABLE(s_tbl_vco2, SVC_UUID_VCO2, CHR_UUID_VCO2, s_val_vco2, s_cccd_vco2);
MAKE_SVC_TABLE(s_tbl_rq, SVC_UUID_RQ, CHR_UUID_RQ, s_val_rq, s_cccd_rq);

/* ================================================================== */
/*  Handle storage                                                      */
/* ================================================================== */

typedef struct
{
    uint16_t svc, chr_decl, chr_val, cccd;
} svc_handles_t;

static svc_handles_t s_srv_hr = {0};
static svc_handles_t s_srv_power = {0};
static svc_handles_t s_srv_vo2max = {0};
static svc_handles_t s_srv_vo2 = {0};
static svc_handles_t s_srv_vco2 = {0};
static svc_handles_t s_srv_rq = {0};

/* ================================================================== */
/*  Server registration state machine                                   */
/* ================================================================== */

typedef enum
{
    SREG_HR = 0,
    SREG_POWER,
    SREG_VO2MAX,
    SREG_VO2,
    SREG_VCO2,
    SREG_RQ,
    SREG_DONE,
} sreg_step_t;

static sreg_step_t s_sreg_step = SREG_HR;

/* ================================================================== */
/*  GATTC sensor context                                                */
/* ================================================================== */

#define GATTC_APP_HR 0
#define GATTC_APP_POWER 1

typedef enum
{
    SENSOR_IDLE = 0,
    SENSOR_CONNECTING,
    SENSOR_CONNECTED,
    SENSOR_DISCOVERING,
    SENSOR_SUBSCRIBED,
} sensor_state_t;

typedef struct
{
    esp_gatt_if_t gattc_if;
    uint16_t conn_id;
    uint16_t char_handle;
    uint16_t cccd_handle;
    esp_bd_addr_t remote_bda;
    sensor_state_t state;
    uint16_t target_svc_uuid;
    uint16_t target_chr_uuid;
    const char *name;
} sensor_ctx_t;

static sensor_ctx_t s_hr_ctx = {
    .gattc_if = ESP_GATT_IF_NONE,
    .state = SENSOR_IDLE,
    .target_svc_uuid = UUID16_HRS,
    .target_chr_uuid = UUID16_HR_MEAS,
    .name = "HR",
};
static sensor_ctx_t s_pwr_ctx = {
    .gattc_if = ESP_GATT_IF_NONE,
    .state = SENSOR_IDLE,
    .target_svc_uuid = UUID16_CPS,
    .target_chr_uuid = UUID16_CP_MEAS,
    .name = "Power",
};

/* ================================================================== */
/*  Shared runtime state                                                */
/* ================================================================== */

static uint16_t s_srv_conn_id = 0xFFFF;
static esp_gatt_if_t s_gatts_if = ESP_GATT_IF_NONE;
static atomic_bool s_srv_connected = false;

static SemaphoreHandle_t s_gatts_reg_sem = NULL;
static SemaphoreHandle_t s_attr_tab_sem = NULL;

/*
 * s_adv_config_done starts at 0 and bits are SET as each config
 * completes — advertising only fires once both bits are set.
 */
static uint8_t s_adv_config_done = 0;
#define ADV_CONFIG_FLAG (1 << 0)
#define SCAN_RSP_CONFIG_FLAG (1 << 1)

/* ================================================================== */
/*  Send queue (thread-safe outbound path for computed values)         */
/* ================================================================== */

typedef enum
{
    SCH_VO2MAX = 0,
    SCH_VO2,
    SCH_VCO2,
    SCH_RQ,
} send_channel_t;

typedef struct
{
    send_channel_t channel;
    float value;
} send_msg_t;

static QueueHandle_t s_send_queue = NULL;
QueueHandle_t g_heartRateQueue = NULL;
QueueHandle_t g_powerQueue = NULL;

/* ================================================================== */
/*  Advertising data                                                    */
/* ================================================================== */

static uint8_t s_adv_uuids[4] = {
    0x0D,
    0x18, /* HRS 0x180D LE */
    0x18,
    0x18, /* CPS 0x1818 LE */
};

static esp_ble_adv_data_t s_adv_data = {
    .set_scan_rsp = false,
    .include_name = false,
    .include_txpower = false,
    .min_interval = 0x0006,
    .max_interval = 0x0010,
    .appearance = 0x0480,
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(s_adv_uuids),
    .p_service_uuid = s_adv_uuids,
    .flag = ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT,
};

static esp_ble_adv_data_t s_scan_rsp_data = {
    .set_scan_rsp = true,
    .include_name = true,
    .include_txpower = true,
    .appearance = 0x0480,
    .flag = ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT,
};

static esp_ble_adv_params_t s_adv_params = {
    .adv_int_min = 0x0020,
    .adv_int_max = 0x0040,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

/* ================================================================== */
/*  Scan parameters                                                     */
/* ================================================================== */

static esp_ble_scan_params_t s_scan_params = {
    .scan_type = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval = 0x0050,
    .scan_window = 0x0030,
    .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE,
};

/* ================================================================== */
/*  Forward declarations                                                */
/* ================================================================== */

static void bridge_task(void *arg);
static void gap_event_handler(esp_gap_ble_cb_event_t, esp_ble_gap_cb_param_t *);
static void gatts_event_handler(esp_gatts_cb_event_t, esp_gatt_if_t, esp_ble_gatts_cb_param_t *);
static void gattc_event_handler(esp_gattc_cb_event_t, esp_gatt_if_t, esp_ble_gattc_cb_param_t *);

static void register_next_server_table(esp_gatt_if_t gatts_if);
static void subscribe_to_sensor(sensor_ctx_t *ctx);
static void relay_hr(const uint8_t *data, uint16_t len);
static void relay_power(const uint8_t *data, uint16_t len);
static void drain_send_queue(void);
static void do_notify_float(uint16_t val_handle, float value);
static sensor_ctx_t *ctx_by_gattc_if(esp_gatt_if_t gattc_if);

/* ================================================================== */
/*  Application callbacks                                               */
/* ================================================================== */

void BLUETOOTH_OnHeartRate(uint16_t bpm)
{
    xQueueOverwrite(g_heartRateQueue, (void *)&bpm);
}

void BLUETOOTH_OnPower(int16_t watts)
{
    xQueueOverwrite(g_powerQueue, (void *)&watts);

    PLOT_DATA(POWER_PLOT_ID, (double)watts);
}

/* ================================================================== */
/*  Public API                                                          */
/* ================================================================== */

void BLUETOOTH_Initialize(void)
{
    g_heartRateQueue = xQueueCreate(1, sizeof(int16_t));
    g_powerQueue = xQueueCreate(1, sizeof(int16_t));
    s_send_queue = xQueueCreate(SEND_QUEUE_DEPTH, sizeof(send_msg_t));
    configASSERT(s_send_queue);
    xTaskCreate(bridge_task, "bridge_task", BRIDGE_TASK_STACK, NULL,
                BRIDGE_TASK_PRIORITY, NULL);
}

bool BLUETOOTH_IsConnected(void) { return atomic_load(&s_srv_connected); }
bool BLUETOOTH_IsHRConnected(void) { return s_hr_ctx.state == SENSOR_SUBSCRIBED; }
bool BLUETOOTH_IsPowerConnected(void) { return s_pwr_ctx.state == SENSOR_SUBSCRIBED; }

/* ---- Internal enqueue helper ---- */
static void enqueue(send_channel_t ch, float value)
{
    if (!atomic_load(&s_srv_connected))
        return;
    send_msg_t msg = {.channel = ch, .value = value};
    if (xQueueSend(s_send_queue, &msg, 0) == errQUEUE_FULL)
    {
        send_msg_t discard;
        xQueueReceive(s_send_queue, &discard, 0);
        xQueueSend(s_send_queue, &msg, 0);
    }
}

void BLUETOOTH_SendVO2Max(float v) { enqueue(SCH_VO2MAX, v); }
void BLUETOOTH_SendVO2(float v) { enqueue(SCH_VO2, v); }
void BLUETOOTH_SendVCO2(float v) { enqueue(SCH_VCO2, v); }
void BLUETOOTH_SendRQ(float v) { enqueue(SCH_RQ, v); }

/* ================================================================== */
/*  Bridge task                                                         */
/* ================================================================== */

static void bridge_task(void *arg)
{
    s_gatts_reg_sem = xSemaphoreCreateBinary();
    s_attr_tab_sem = xSemaphoreCreateBinary();
    configASSERT(s_gatts_reg_sem && s_attr_tab_sem);

    /* ---- Bluedroid init ---- */
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

    esp_bluedroid_config_t bd_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bluedroid_init_with_cfg(&bd_cfg));
    ESP_ERROR_CHECK(esp_bluedroid_enable());
    vTaskDelay(pdMS_TO_TICKS(500));

    /* ---- Register callbacks ---- */
    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts_event_handler));
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));
    ESP_ERROR_CHECK(esp_ble_gattc_register_callback(gattc_event_handler));

    /* ---- GATTS app registration — wait for REG_EVT ---- */
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(0));
    xSemaphoreTake(s_gatts_reg_sem, portMAX_DELAY);
    ESP_LOGI(TAG, "GATTS registered (if=%d)", s_gatts_if);

    /* ---- GATTC app registrations (one per sensor) ---- */
    ESP_ERROR_CHECK(esp_ble_gattc_app_register(GATTC_APP_HR));
    ESP_ERROR_CHECK(esp_ble_gattc_app_register(GATTC_APP_POWER));

    /* ---- MTU ---- */
    ESP_ERROR_CHECK(esp_ble_gatt_set_local_mtu(247));

    /*
     * Set device name then give the GAP stack a moment to settle before
     * configuring adv data. Using a short delay here is more portable
     * than relying on ESP_GAP_BLE_LOCAL_NAME_SET_COMPLETE_EVT which is
     * not present in all IDF v6 builds.
     */
    ESP_ERROR_CHECK(esp_ble_gap_set_device_name(BLE_DEVICE_NAME));
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_ble_gap_config_adv_data(&s_adv_data);
    esp_ble_gap_config_adv_data(&s_scan_rsp_data);

    /* ---- Register all 6 GATT server services sequentially ---- */
    while (s_sreg_step != SREG_DONE)
    {
        register_next_server_table(s_gatts_if);
        xSemaphoreTake(s_attr_tab_sem, portMAX_DELAY);
    }
    ESP_LOGI(TAG, "All 6 server services registered");

    /*
     * Start scanning unconditionally after services are up.
     * Sensors connect on their own conn_ids; the downstream advertising
     * slot remains available for incoming connections in parallel.
     */
    ESP_LOGI(TAG, "Starting sensor scan");
    esp_ble_gap_set_scan_params(&s_scan_params);

    /* ---- Main loop: drain outbound queue + periodic status log ---- */

    while (1)
    {
        drain_send_queue();
        vTaskDelay(pdMS_TO_TICKS(DRAIN_PERIOD_MS));
    }
}

/* ================================================================== */
/*  Server service registration chain                                   */
/* ================================================================== */

static void register_next_server_table(esp_gatt_if_t gatts_if)
{
    switch (s_sreg_step)
    {
    case SREG_HR:
        ESP_LOGI(TAG, "Registering HR service (inst 0)");
        esp_ble_gatts_create_attr_tab(s_tbl_hr, gatts_if, SVC_TABLE_SIZE, 0);
        s_sreg_step = SREG_POWER;
        break;
    case SREG_POWER:
        ESP_LOGI(TAG, "Registering Power service (inst 1)");
        esp_ble_gatts_create_attr_tab(s_tbl_power, gatts_if, SVC_TABLE_SIZE, 1);
        s_sreg_step = SREG_VO2MAX;
        break;
    case SREG_VO2MAX:
        ESP_LOGI(TAG, "Registering VO2Max service (inst 2)");
        esp_ble_gatts_create_attr_tab(s_tbl_vo2max, gatts_if, SVC_TABLE_SIZE, 2);
        s_sreg_step = SREG_VO2;
        break;
    case SREG_VO2:
        ESP_LOGI(TAG, "Registering VO2 service (inst 3)");
        esp_ble_gatts_create_attr_tab(s_tbl_vo2, gatts_if, SVC_TABLE_SIZE, 3);
        s_sreg_step = SREG_VCO2;
        break;
    case SREG_VCO2:
        ESP_LOGI(TAG, "Registering VCO2 service (inst 4)");
        esp_ble_gatts_create_attr_tab(s_tbl_vco2, gatts_if, SVC_TABLE_SIZE, 4);
        s_sreg_step = SREG_RQ;
        break;
    case SREG_RQ:
        ESP_LOGI(TAG, "Registering RQ service (inst 5)");
        esp_ble_gatts_create_attr_tab(s_tbl_rq, gatts_if, SVC_TABLE_SIZE, 5);
        s_sreg_step = SREG_DONE;
        break;
    case SREG_DONE:
        break;
    }
}

/* ================================================================== */
/*  Outbound queue drain                                                */
/* ================================================================== */

static void drain_send_queue(void)
{
    if (!atomic_load(&s_srv_connected))
        return;
    if (s_gatts_if == ESP_GATT_IF_NONE)
        return;

    send_msg_t msg;
    while (xQueueReceive(s_send_queue, &msg, 0) == pdTRUE)
    {
        uint16_t handle = 0;
        switch (msg.channel)
        {
        case SCH_VO2MAX:
            handle = s_srv_vo2max.chr_val;
            break;
        case SCH_VO2:
            handle = s_srv_vo2.chr_val;
            break;
        case SCH_VCO2:
            handle = s_srv_vco2.chr_val;
            break;
        case SCH_RQ:
            handle = s_srv_rq.chr_val;
            break;
        default:
            continue;
        }
        if (handle == 0)
            continue;
        do_notify_float(handle, msg.value);
    }
}

static void do_notify_float(uint16_t val_handle, float value)
{
    uint8_t buf[4];
    memcpy(buf, &value, 4);
    esp_ble_gatts_send_indicate(s_gatts_if, s_srv_conn_id,
                                val_handle, sizeof(buf), buf, false);
}

/* ================================================================== */
/*  GAP event handler                                                   */
/* ================================================================== */

/*
 * Helper: walk one AD payload buffer looking for 16-bit service UUIDs.
 * Sets *found_hrs / *found_cps if 0x180D / 0x1818 is present.
 */
static void parse_adv_uuids(const uint8_t *buf, uint8_t buflen,
                            bool *found_hrs, bool *found_cps)
{
    for (uint8_t i = 0; i + 1 < buflen;)
    {
        uint8_t len = buf[i];
        uint8_t type = buf[i + 1];
        if (len == 0)
        {
            i++;
            continue;
        }
        if (type == 0x02 || type == 0x03)
        {
            for (uint8_t j = 2; j + 1 <= len && i + j + 1 < buflen; j += 2)
            {
                uint16_t uuid = buf[i + j] | ((uint16_t)buf[i + j + 1] << 8);
                if (uuid == UUID16_HRS)
                    *found_hrs = true;
                if (uuid == UUID16_CPS)
                    *found_cps = true;
            }
        }
        i += len + 1;
    }
}

static void gap_event_handler(esp_gap_ble_cb_event_t event,
                              esp_ble_gap_cb_param_t *param)
{
    switch (event)
    {
    /* ── Advertising side ── */

    /*
     * Set the bit on completion; start advertising only when BOTH
     * configs are done. adv config is initiated from bridge_task after
     * the device name is set, so the stack is guaranteed ready by then.
     */
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        s_adv_config_done |= ADV_CONFIG_FLAG;
        if (s_adv_config_done == (ADV_CONFIG_FLAG | SCAN_RSP_CONFIG_FLAG))
            esp_ble_gap_start_advertising(&s_adv_params);
        break;

    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        s_adv_config_done |= SCAN_RSP_CONFIG_FLAG;
        if (s_adv_config_done == (ADV_CONFIG_FLAG | SCAN_RSP_CONFIG_FLAG))
            esp_ble_gap_start_advertising(&s_adv_params);
        break;

    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
            ESP_LOGE(TAG, "Adv start failed: %d", param->adv_start_cmpl.status);
        else
            ESP_LOGI(TAG, "Advertising started");
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
            ESP_LOGE(TAG, "Adv stop failed: %d", param->adv_stop_cmpl.status);
        break;

    /* ── Security: accept pairing requests from the downstream device ── */
    case ESP_GAP_BLE_SEC_REQ_EVT:
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
        break;

    /* ── Scanning side ── */
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        ESP_LOGI(TAG, "Scan params set — starting scan");
        esp_ble_gap_start_scanning(0);
        break;

    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
            ESP_LOGE(TAG, "Scan start failed: %d", param->scan_start_cmpl.status);
        break;

    case ESP_GAP_BLE_SCAN_RESULT_EVT:
    {
        if (param->scan_rst.search_evt != ESP_GAP_SEARCH_INQ_RES_EVT)
            break;

        bool found_hrs = false, found_cps = false;

        /* Primary advertising payload */
        parse_adv_uuids(param->scan_rst.ble_adv,
                        param->scan_rst.adv_data_len,
                        &found_hrs, &found_cps);

        /* Scan response payload — concatenated after primary adv bytes */
        if (param->scan_rst.scan_rsp_len > 0)
        {
            parse_adv_uuids(param->scan_rst.ble_adv + param->scan_rst.adv_data_len,
                            param->scan_rst.scan_rsp_len,
                            &found_hrs, &found_cps);
        }

        if (found_hrs && s_hr_ctx.state == SENSOR_IDLE && s_hr_ctx.gattc_if != ESP_GATT_IF_NONE)
        {
            ESP_LOGI(TAG, "Found HR monitor — connecting");
            s_hr_ctx.state = SENSOR_CONNECTING;
            memcpy(s_hr_ctx.remote_bda, param->scan_rst.bda, sizeof(esp_bd_addr_t));
            esp_ble_gap_stop_scanning();
            esp_ble_gattc_open(s_hr_ctx.gattc_if,
                               param->scan_rst.bda,
                               param->scan_rst.ble_addr_type, true);
        }
        else if (found_cps && s_pwr_ctx.state == SENSOR_IDLE && s_pwr_ctx.gattc_if != ESP_GATT_IF_NONE)
        {
            ESP_LOGI(TAG, "Found power meter — connecting");
            s_pwr_ctx.state = SENSOR_CONNECTING;
            memcpy(s_pwr_ctx.remote_bda, param->scan_rst.bda, sizeof(esp_bd_addr_t));
            esp_ble_gap_stop_scanning();
            esp_ble_gattc_open(s_pwr_ctx.gattc_if,
                               param->scan_rst.bda,
                               param->scan_rst.ble_addr_type, true);
        }
        break;
    }

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        if (s_hr_ctx.state < SENSOR_CONNECTED ||
            s_pwr_ctx.state < SENSOR_CONNECTED)
        {
            vTaskDelay(pdMS_TO_TICKS(200));
            esp_ble_gap_start_scanning(0);
        }
        break;

    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        ESP_LOGI(TAG, "Conn params updated: interval=%d latency=%d timeout=%d",
                 param->update_conn_params.conn_int,
                 param->update_conn_params.latency,
                 param->update_conn_params.timeout);
        break;

    default:
        break;
    }
}

/* ================================================================== */
/*  GATTS event handler (server — downstream to smartwatch)            */
/* ================================================================== */

static void gatts_event_handler(esp_gatts_cb_event_t event,
                                esp_gatt_if_t gatts_if,
                                esp_ble_gatts_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GATTS_REG_EVT:
        if (param->reg.status != ESP_GATT_OK)
        {
            ESP_LOGE(TAG, "GATTS reg failed: %d", param->reg.status);
            break;
        }
        s_gatts_if = gatts_if;
        xSemaphoreGive(s_gatts_reg_sem);
        break;

    case ESP_GATTS_CREAT_ATTR_TAB_EVT:
        if (param->add_attr_tab.status != ESP_GATT_OK)
        {
            ESP_LOGE(TAG, "Attr tab failed inst=%d status=%d",
                     param->add_attr_tab.svc_inst_id,
                     param->add_attr_tab.status);
            break;
        }
        if (param->add_attr_tab.num_handle != SVC_TABLE_SIZE)
        {
            ESP_LOGE(TAG, "Attr tab size mismatch inst=%d got=%d expected=%d",
                     param->add_attr_tab.svc_inst_id,
                     param->add_attr_tab.num_handle, SVC_TABLE_SIZE);
            break;
        }

        switch (param->add_attr_tab.svc_inst_id)
        {
        case 0:
            s_srv_hr.svc = param->add_attr_tab.handles[IDX_SVC];
            s_srv_hr.chr_decl = param->add_attr_tab.handles[IDX_CHR_DECL];
            s_srv_hr.chr_val = param->add_attr_tab.handles[IDX_CHR_VAL];
            s_srv_hr.cccd = param->add_attr_tab.handles[IDX_CCCD];
            esp_ble_gatts_start_service(s_srv_hr.svc);
            break;
        case 1:
            s_srv_power.svc = param->add_attr_tab.handles[IDX_SVC];
            s_srv_power.chr_decl = param->add_attr_tab.handles[IDX_CHR_DECL];
            s_srv_power.chr_val = param->add_attr_tab.handles[IDX_CHR_VAL];
            s_srv_power.cccd = param->add_attr_tab.handles[IDX_CCCD];
            esp_ble_gatts_start_service(s_srv_power.svc);
            break;
        case 2:
            s_srv_vo2max.svc = param->add_attr_tab.handles[IDX_SVC];
            s_srv_vo2max.chr_decl = param->add_attr_tab.handles[IDX_CHR_DECL];
            s_srv_vo2max.chr_val = param->add_attr_tab.handles[IDX_CHR_VAL];
            s_srv_vo2max.cccd = param->add_attr_tab.handles[IDX_CCCD];
            esp_ble_gatts_start_service(s_srv_vo2max.svc);
            break;
        case 3:
            s_srv_vo2.svc = param->add_attr_tab.handles[IDX_SVC];
            s_srv_vo2.chr_decl = param->add_attr_tab.handles[IDX_CHR_DECL];
            s_srv_vo2.chr_val = param->add_attr_tab.handles[IDX_CHR_VAL];
            s_srv_vo2.cccd = param->add_attr_tab.handles[IDX_CCCD];
            esp_ble_gatts_start_service(s_srv_vo2.svc);
            break;
        case 4:
            s_srv_vco2.svc = param->add_attr_tab.handles[IDX_SVC];
            s_srv_vco2.chr_decl = param->add_attr_tab.handles[IDX_CHR_DECL];
            s_srv_vco2.chr_val = param->add_attr_tab.handles[IDX_CHR_VAL];
            s_srv_vco2.cccd = param->add_attr_tab.handles[IDX_CCCD];
            esp_ble_gatts_start_service(s_srv_vco2.svc);
            break;
        case 5:
            s_srv_rq.svc = param->add_attr_tab.handles[IDX_SVC];
            s_srv_rq.chr_decl = param->add_attr_tab.handles[IDX_CHR_DECL];
            s_srv_rq.chr_val = param->add_attr_tab.handles[IDX_CHR_VAL];
            s_srv_rq.cccd = param->add_attr_tab.handles[IDX_CCCD];
            esp_ble_gatts_start_service(s_srv_rq.svc);
            break;
        default:
            ESP_LOGW(TAG, "Unknown svc_inst_id %d", param->add_attr_tab.svc_inst_id);
            break;
        }
        xSemaphoreGive(s_attr_tab_sem);
        break;

    case ESP_GATTS_START_EVT:
        ESP_LOGI(TAG, "Service started, handle %d", param->start.service_handle);
        break;

    case ESP_GATTS_CONNECT_EVT:
        ESP_LOGI(TAG, "Downstream connected, conn_id=%d", param->connect.conn_id);
        s_srv_conn_id = param->connect.conn_id;
        atomic_store(&s_srv_connected, true);
        {
            esp_ble_conn_update_params_t cp = {0};
            memcpy(cp.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
            cp.latency = 0;
            cp.min_int = 0x10;
            cp.max_int = 0x20;
            cp.timeout = 400;
            esp_ble_gap_update_conn_params(&cp);
        }
        esp_ble_gap_start_advertising(&s_adv_params);
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(TAG, "Downstream disconnected, reason=0x%02x", param->disconnect.reason);
        atomic_store(&s_srv_connected, false);
        esp_ble_gap_start_advertising(&s_adv_params);
        break;

    case ESP_GATTS_WRITE_EVT:
        if (param->write.need_rsp)
            esp_ble_gatts_send_response(gatts_if, param->write.conn_id,
                                        param->write.trans_id, ESP_GATT_OK, NULL);
        break;

    case ESP_GATTS_MTU_EVT:
        ESP_LOGI(TAG, "MTU negotiated: %d", param->mtu.mtu);
        break;

    case ESP_GATTS_CONF_EVT:
        if (param->conf.status != ESP_GATT_OK)
            ESP_LOGW(TAG, "Notify confirm failed handle=%d status=%d",
                     param->conf.handle, param->conf.status);
        break;

    default:
        break;
    }
}

/* ================================================================== */
/*  GATTC event handler (client — upstream from sensors)               */
/* ================================================================== */

static sensor_ctx_t *ctx_by_gattc_if(esp_gatt_if_t gattc_if)
{
    if (s_hr_ctx.gattc_if == gattc_if)
        return &s_hr_ctx;
    if (s_pwr_ctx.gattc_if == gattc_if)
        return &s_pwr_ctx;
    return NULL;
}

static void gattc_event_handler(esp_gattc_cb_event_t event,
                                esp_gatt_if_t gattc_if,
                                esp_ble_gattc_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GATTC_REG_EVT:
        if (param->reg.status != ESP_GATT_OK)
        {
            ESP_LOGE(TAG, "GATTC reg failed app_id=%d", param->reg.app_id);
            break;
        }
        if (param->reg.app_id == GATTC_APP_HR)
            s_hr_ctx.gattc_if = gattc_if;
        else if (param->reg.app_id == GATTC_APP_POWER)
            s_pwr_ctx.gattc_if = gattc_if;
        ESP_LOGI(TAG, "GATTC app_id=%d registered, if=%d",
                 param->reg.app_id, gattc_if);
        break;

    case ESP_GATTC_CONNECT_EVT:
    {
        sensor_ctx_t *ctx = ctx_by_gattc_if(gattc_if);
        if (!ctx)
            break;
        ctx->conn_id = param->connect.conn_id;
        ctx->state = SENSOR_DISCOVERING;
        ESP_LOGI(TAG, "%s connected, conn_id=%d — discovering", ctx->name, ctx->conn_id);
        esp_ble_gattc_search_service(gattc_if, ctx->conn_id, NULL);
        break;
    }

    /*
     * SEARCH_RES_EVT: just note that the target service was seen.
     * Do NOT call get_char_by_uuid here — the cache is not yet valid.
     */
    case ESP_GATTC_SEARCH_RES_EVT:
    {
        sensor_ctx_t *ctx = ctx_by_gattc_if(gattc_if);
        if (!ctx)
            break;
        ESP_LOGI(TAG, "%s: service found uuid_len=%d uuid16=0x%04x",
                 ctx->name,
                 param->search_res.srvc_id.uuid.len,
                 param->search_res.srvc_id.uuid.uuid.uuid16);
        break;
    }

    /*
     * SEARCH_CMPL_EVT: cache is now valid — look up char and CCCD here.
     */
    case ESP_GATTC_SEARCH_CMPL_EVT:
    {
        sensor_ctx_t *ctx = ctx_by_gattc_if(gattc_if);
        ESP_LOGI(TAG, "SEARCH_CMPL if=%d ctx=%s status=%d",
                 gattc_if,
                 ctx ? ctx->name : "NULL",
                 param->search_cmpl.status);
        if (!ctx)
            break;

        if (param->search_cmpl.status != ESP_GATT_OK)
        {
            ESP_LOGE(TAG, "%s: service discovery failed status=%d",
                     ctx->name, param->search_cmpl.status);
            break;
        }

        uint16_t count = 10;
        esp_gattc_char_elem_t chars[10];
        esp_bt_uuid_t chr_uuid = {
            .len = ESP_UUID_LEN_16,
            .uuid.uuid16 = ctx->target_chr_uuid,
        };

        esp_gatt_status_t st = esp_ble_gattc_get_char_by_uuid(
            gattc_if, ctx->conn_id,
            0x0001, 0xFFFF,
            chr_uuid, chars, &count);

        ESP_LOGI(TAG, "%s: get_char_by_uuid st=%d count=%d", ctx->name, st, count);
        for (int i = 0; i < count; i++)
            ESP_LOGI(TAG, "  char[%d] handle=%d prop=0x%02x",
                     i, chars[i].char_handle, chars[i].properties);

        if (st != ESP_GATT_OK || count == 0)
        {
            ESP_LOGW(TAG, "%s: characteristic not found st=%d", ctx->name, st);
            break;
        }

        ctx->char_handle = chars[0].char_handle;
        ESP_LOGI(TAG, "%s: char handle=%d", ctx->name, ctx->char_handle);

        /* Locate the CCCD descriptor */
        uint16_t desc_count = 1;
        esp_gattc_descr_elem_t desc;
        esp_bt_uuid_t cccd_uuid = {
            .len = ESP_UUID_LEN_16,
            .uuid.uuid16 = UUID16_CCCD,
        };
        esp_gatt_status_t dsc_st = esp_ble_gattc_get_descr_by_char_handle(
            gattc_if, ctx->conn_id, ctx->char_handle,
            cccd_uuid, &desc, &desc_count);
        ESP_LOGI(TAG, "%s: get_descr st=%d count=%d", ctx->name, dsc_st, desc_count);

        if (dsc_st == ESP_GATT_OK && desc_count > 0)
        {
            ctx->cccd_handle = desc.handle;
            ESP_LOGI(TAG, "%s: CCCD handle=%d", ctx->name, ctx->cccd_handle);
        }

        subscribe_to_sensor(ctx);
        break;
    }

    case ESP_GATTC_NOTIFY_EVT:
    {
        sensor_ctx_t *ctx = ctx_by_gattc_if(gattc_if);
        if (!ctx || param->notify.handle != ctx->char_handle)
            break;
        if (ctx == &s_hr_ctx)
            relay_hr(param->notify.value, param->notify.value_len);
        else
            relay_power(param->notify.value, param->notify.value_len);
        break;
    }

    case ESP_GATTC_WRITE_DESCR_EVT:
    {
        sensor_ctx_t *ctx = ctx_by_gattc_if(gattc_if);
        if (!ctx)
            break;
        if (param->write.status == ESP_GATT_OK)
        {
            ctx->state = SENSOR_SUBSCRIBED;
            ESP_LOGI(TAG, "%s: notifications enabled", ctx->name);
        }
        else
        {
            ESP_LOGE(TAG, "%s: CCCD write failed status=%d",
                     ctx->name, param->write.status);
        }
        break;
    }

    case ESP_GATTC_DISCONNECT_EVT:
    {
        sensor_ctx_t *ctx = ctx_by_gattc_if(gattc_if);
        if (!ctx)
            break;
        ESP_LOGW(TAG, "%s sensor disconnected reason=0x%02x — will rescan",
                 ctx->name, param->disconnect.reason);
        ctx->state = SENSOR_IDLE;
        ctx->char_handle = 0;
        ctx->cccd_handle = 0;
        esp_ble_gap_start_scanning(0);
        break;
    }

    default:
        break;
    }
}

/* ================================================================== */
/*  subscribe_to_sensor                                                 */
/* ================================================================== */

static void subscribe_to_sensor(sensor_ctx_t *ctx)
{
    if (ctx->cccd_handle == 0)
    {
        ctx->cccd_handle = ctx->char_handle + 1;
        ESP_LOGW(TAG, "%s: CCCD not found by discovery, trying handle %d",
                 ctx->name, ctx->cccd_handle);
    }

    /*
     * [FIX] Register with Bluedroid's internal notify dispatch table
     * BEFORE writing the CCCD. Without this call Bluedroid silently
     * discards incoming ATT notifications and ESP_GATTC_NOTIFY_EVT
     * never fires, even though the peripheral is sending data and the
     * CCCD write succeeds.
     */
    esp_err_t ret = esp_ble_gattc_register_for_notify(
        ctx->gattc_if, ctx->remote_bda, ctx->char_handle);
    if (ret != ESP_OK)
        ESP_LOGE(TAG, "%s: register_for_notify failed: %s",
                 ctx->name, esp_err_to_name(ret));

    /* Tell the peripheral to start sending notifications */
    uint8_t notify_en[2] = {0x01, 0x00};
    esp_ble_gattc_write_char_descr(
        ctx->gattc_if, ctx->conn_id, ctx->cccd_handle,
        sizeof(notify_en), notify_en,
        ESP_GATT_WRITE_TYPE_RSP,
        ESP_GATT_AUTH_REQ_NONE);
}

/* ================================================================== */
/*  Relay functions                                                     */
/* ================================================================== */

static void relay_hr(const uint8_t *data, uint16_t len)
{
    if (len < 2)
        return;

    if (atomic_load(&s_srv_connected) && s_srv_hr.chr_val != 0)
    {
        esp_ble_gatts_send_indicate(s_gatts_if, s_srv_conn_id,
                                    s_srv_hr.chr_val,
                                    len, (uint8_t *)data, false);
    }

    uint16_t bpm = (data[0] & 0x01)
                       ? (data[1] | ((uint16_t)data[2] << 8))
                       : data[1];
    BLUETOOTH_OnHeartRate(bpm);
    ESP_LOGD(TAG, "HR relay: %u bpm", bpm);
}

static void relay_power(const uint8_t *data, uint16_t len)
{

    if (len < 4)
        return;

    if (atomic_load(&s_srv_connected) && s_srv_power.chr_val != 0)
    {
        esp_ble_gatts_send_indicate(s_gatts_if, s_srv_conn_id,
                                    s_srv_power.chr_val,
                                    len, (uint8_t *)data, false);
    }

    int16_t watts = (int16_t)((uint16_t)data[2] | ((uint16_t)data[3] << 8));

    BLUETOOTH_OnPower(watts);
}