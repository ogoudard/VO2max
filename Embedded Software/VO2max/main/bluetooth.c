#include "bluetooth.h"
#include "esp_bt.h"
#include "esp_log.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_bt_defs.h"
#include "esp_gatt_common_api.h"
#include "esp_bt_main.h"

const char *TAG = "[BLE]";

#define BLUETOOTH_TASK_STACK_SIZE 8192
#define BLUETOOTH_TASK_PRIORITY 0
#define BLUETOOTH_TASK_PERIOD_MS 1000

#define CONFIG_EXAMPLE_CHAR_READ_DATA_LEN 3

#define VO2MAX_RATE_SERVICE_UUID "b354cf1b-2486-4f21-b4b1-ee4cd5cc3bf0"

// Vo2 service
#define VO2_RATE_SERVICE_UUID "231c616b-32a6-4b93-9f0c-fe728deca0a5"

// Vco2 service
#define VCO2_RATE_SERVIC_UUIDE "196c1c76-fe53-47e7-baab-a32b404d63df"

// RQ service
#define RQ_RATE_SERVICE_UUID "8868ba7f-cada-4035-85d2-e0002aeb2be6"

// O2perc service
#define O2_RATE_SERVICE_UUID "f362445d-0824-40e1-a53a-8e90106f0ba3"

// C02perc service
#define CO2C_RATE_SERVICE "4b1ad6ec-d8d3-426c-8d1e-0dea4d6b4ebc")

// kcal service
#define KCAL_RATE_SERVICE_UUID "3483923e-e94a-4a21-96c4-c12a442ee4cf")

#define GATTS_CHAR_UUID_TEST_A 0xFF01
#define GATTS_DESCR_UUID_TEST_A 0x3333
#define GATTS_NUM_HANDLE_TEST_A 4

#define PROFILE_NUM 1
#define PROFILE_APP_ID 0

#define adv_config_flag (1 << 0)
#define scan_rsp_config_flag (1 << 1)

static char test_device_name[ESP_BLE_ADV_NAME_LEN_MAX] = "VO2MAX";

#define TEST_MANUFACTURER_DATA_LEN 17

#define GATTS_DEMO_CHAR_VAL_LEN_MAX 0x40

#define PREPARE_BUF_MAX_SIZE 1024

static uint8_t char1_str[CONFIG_EXAMPLE_CHAR_READ_DATA_LEN] = {0x11, 0x22, 0x33};

static uint16_t descr_value = 0x0;
/**
 * Current MTU size for the active connection.
 *
 * This simplified implementation assumes a single connection.
 * For multi-connection scenarios, the MTU should be stored per connection ID.
 */
static uint16_t local_mtu = 23;

static uint8_t char_value_read[] = {0xDE, 0xED, 0xBE, 0xEF};

static esp_gatt_char_prop_t a_property = 0;
static esp_gatt_char_prop_t b_property = 0;

typedef struct
{
    uint8_t *prepare_buf;
    int prepare_len;
} prepare_type_env_t;

static esp_attr_value_t gatts_demo_char1_val =
    {
        .attr_max_len = GATTS_DEMO_CHAR_VAL_LEN_MAX,
        .attr_len = sizeof(char1_str),
        .attr_value = char1_str,
};

static uint8_t adv_config_done = 0;

static const uint8_t gattsServiceCo2Uuid[32] = {0xBC, 0x4E, 0x6B, 0x4D, 0xEA, 0x0D, 0x1E, 0x8D, 0x6C, 0x42, 0xD3, 0xD8, 0xEC, 0xD6, 0x1A, 0x4B};

static uint8_t adv_service_uuid128[32] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    // first uuid, 16bit, [12],[13] is the value
    0xfb,
    0x34,
    0x9b,
    0x5f,
    0x80,
    0x00,
    0x00,
    0x80,
    0x00,
    0x10,
    0x00,
    0x00,
    0xEE,
    0x00,
    0x00,
    0x00,
    // second uuid, 32bit, [12], [13], [14], [15] is the value
    0xfb,
    0x34,
    0x9b,
    0x5f,
    0x80,
    0x00,
    0x00,
    0x80,
    0x00,
    0x10,
    0x00,
    0x00,
    0xFF,
    0x00,
    0x00,
    0x00,
};

// The length of adv data must be less than 31 bytes
// static uint8_t test_manufacturer[TEST_MANUFACTURER_DATA_LEN] =  {0x12, 0x23, 0x45, 0x56};
// adv data
static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = false,
    .min_interval = ESP_BLE_GAP_CONN_ITVL_MS(7.5), // slave connection min interval
    .max_interval = ESP_BLE_GAP_CONN_ITVL_MS(20),  // slave connection max interval
    .appearance = 0x00,
    .manufacturer_len = 0,       // TEST_MANUFACTURER_DATA_LEN,
    .p_manufacturer_data = NULL, //&test_manufacturer[0],
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(adv_service_uuid128),
    .p_service_uuid = adv_service_uuid128,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};
// scan response data
static esp_ble_adv_data_t scan_rsp_data = {
    .set_scan_rsp = true,
    .include_name = true,
    .include_txpower = true,
    //.min_interval = 0x0006,
    //.max_interval = 0x0010,
    .appearance = 0x00,
    .manufacturer_len = 0,       // TEST_MANUFACTURER_DATA_LEN,
    .p_manufacturer_data = NULL, //&test_manufacturer[0],
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(adv_service_uuid128),
    .p_service_uuid = adv_service_uuid128,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min = ESP_BLE_GAP_ADV_ITVL_MS(20),
    .adv_int_max = ESP_BLE_GAP_ADV_ITVL_MS(40),
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    //.peer_addr            =
    //.peer_addr_type       =
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static prepare_type_env_t a_prepare_write_env;
static prepare_type_env_t b_prepare_write_env;

struct gatts_profile_inst
{
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};

static TaskHandle_t bluetoothTaskHandle;

static void BluetoothTask(void *pvParameters);
static bool InitializeLowLevel();
static void GapEventHandler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void GattsProfileEventHandler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void GattsEventHandler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

/* One gatt-based profile one app_id and one gatts_if, this array will store the gatts_if returned by ESP_GATTS_REG_EVT */
static struct gatts_profile_inst gl_profile = {
    .gatts_cb = GattsProfileEventHandler,
    .gatts_if = ESP_GATT_IF_NONE, /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
};

void BLUETOOTH_Initialize()
{
    xTaskCreate(BluetoothTask, "Bluetooth task", BLUETOOTH_TASK_STACK_SIZE, NULL, BLUETOOTH_TASK_PRIORITY, &bluetoothTaskHandle);
}

static void BluetoothTask(void *pvParameters)
{
    const char *bleTaskTag = "[BLE Task]";

    ESP_LOGI(bleTaskTag, "Task started");
    ESP_LOGI(bleTaskTag, "Initialize low level");
    InitializeLowLevel();

    ESP_LOGI(bleTaskTag, "Create services");

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(BLUETOOTH_TASK_PERIOD_MS));
    }
}

static bool InitializeLowLevel()
{
    bool ret = false;

    esp_bt_controller_config_t btConfig = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bluedroid_config_t cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_bt_controller_init(&btConfig));

    while (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE)
    {
    };

    if (esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_INITED)
    {
        ESP_LOGE(TAG, "Failed initializing BLE");
    }
    else
    {
        ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

        if (esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_ENABLED)
        {
            ESP_LOGE(TAG, "Failed initializing BLE");
        }
        else
        {
            if (ESP_OK != esp_bluedroid_init_with_cfg(&cfg))
            {
                ESP_LOGE(TAG, "Bluedroid init failed");
            }
            else
            {
                if (ESP_OK != esp_bluedroid_enable())
                {
                    ESP_LOGE(TAG, "Bluedroid enable failed");
                }
                else
                {
                    if (ESP_OK != esp_ble_gatts_register_callback(GattsEventHandler))
                    {
                        ESP_LOGE(TAG, "gatts event handler register failed");
                    }
                    else
                    {
                        if (ESP_OK != esp_ble_gap_register_callback(GapEventHandler))
                        {
                            ESP_LOGE(TAG, "gap register error");
                        }
                        else
                        {
                            if (ESP_OK != esp_ble_gatts_app_register(PROFILE_APP_ID))
                            {
                                ESP_LOGE(TAG, "gatts app register error");
                            }
                            else
                            {
                                if (ESP_OK != esp_ble_gatt_set_local_mtu(500))
                                {
                                    ESP_LOGE(TAG, "set local  MTU failed, error code");
                                }
                                else
                                {
                                    ret = true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return ret;
}

static void GapEventHandler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~adv_config_flag);
        if (adv_config_done == 0)
        {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~scan_rsp_config_flag);
        if (adv_config_done == 0)
        {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        // advertising start complete event to indicate advertising start successfully or failed
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(TAG, "Advertising start failed, status %d", param->adv_start_cmpl.status);
            break;
        }
        ESP_LOGI(TAG, "Advertising start successfully");
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(TAG, "Advertising stop failed, status %d", param->adv_stop_cmpl.status);
            break;
        }
        ESP_LOGI(TAG, "Advertising stop successfully");
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        ESP_LOGI(TAG, "Connection params update, status %d, conn_int %d, latency %d, timeout %d",
                 param->update_conn_params.status,
                 param->update_conn_params.conn_int,
                 param->update_conn_params.latency,
                 param->update_conn_params.timeout);
        break;
    case ESP_GAP_BLE_SET_PKT_LENGTH_COMPLETE_EVT:
        ESP_LOGI(TAG, "Packet length update, status %d, rx %d, tx %d",
                 param->pkt_data_length_cmpl.status,
                 param->pkt_data_length_cmpl.params.rx_len,
                 param->pkt_data_length_cmpl.params.tx_len);
        break;
    default:
        break;
    }
}

void example_write_event_env(esp_gatt_if_t gatts_if, prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param)
{
    esp_gatt_status_t status = ESP_GATT_OK;
    if (param->write.need_rsp)
    {
        if (param->write.is_prep)
        {
            if (param->write.offset > PREPARE_BUF_MAX_SIZE)
            {
                status = ESP_GATT_INVALID_OFFSET;
            }
            else if ((param->write.offset + param->write.len) > PREPARE_BUF_MAX_SIZE)
            {
                status = ESP_GATT_INVALID_ATTR_LEN;
            }
            if (status == ESP_GATT_OK && prepare_write_env->prepare_buf == NULL)
            {
                prepare_write_env->prepare_buf = (uint8_t *)malloc(PREPARE_BUF_MAX_SIZE * sizeof(uint8_t));
                prepare_write_env->prepare_len = 0;
                if (prepare_write_env->prepare_buf == NULL)
                {
                    ESP_LOGE(TAG, "Gatt_server prep no mem");
                    status = ESP_GATT_NO_RESOURCES;
                }
            }

            // Security fix: Use calloc to ensure memory is zero-initialized
            esp_gatt_rsp_t *gatt_rsp = (esp_gatt_rsp_t *)calloc(1, sizeof(esp_gatt_rsp_t));
            if (gatt_rsp)
            {
                gatt_rsp->attr_value.len = param->write.len;
                gatt_rsp->attr_value.handle = param->write.handle;
                gatt_rsp->attr_value.offset = param->write.offset;
                gatt_rsp->attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;
                memcpy(gatt_rsp->attr_value.value, param->write.value, param->write.len);
                esp_err_t response_err = esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, gatt_rsp);
                if (response_err != ESP_OK)
                {
                    ESP_LOGE(TAG, "Send response error\n");
                }
                free(gatt_rsp);
            }
            else
            {
                ESP_LOGE(TAG, "malloc failed, no resource to send response error\n");
                status = ESP_GATT_NO_RESOURCES;
            }
            if (status != ESP_GATT_OK)
            {
                return;
            }
            memcpy(prepare_write_env->prepare_buf + param->write.offset,
                   param->write.value,
                   param->write.len);
            prepare_write_env->prepare_len += param->write.len;
        }
        else
        {
            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, NULL);
        }
    }
}

void example_exec_write_event_env(prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param)
{
    if (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC)
    {
        ESP_LOG_BUFFER_HEX(TAG, prepare_write_env->prepare_buf, prepare_write_env->prepare_len);
    }
    else
    {
        ESP_LOGI(TAG, "Prepare write cancel");
    }
    if (prepare_write_env->prepare_buf)
    {
        free(prepare_write_env->prepare_buf);
        prepare_write_env->prepare_buf = NULL;
    }
    prepare_write_env->prepare_len = 0;
}

static void GattsProfileEventHandler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GATTS_REG_EVT:
        ESP_LOGI(TAG, "GATT server register, status %d, app_id %d, gatts_if %d", param->reg.status, param->reg.app_id, gatts_if);
        gl_profile.service_id.is_primary = true;
        gl_profile.service_id.id.inst_id = 0x00;
        gl_profile.service_id.id.uuid.len = ESP_UUID_LEN_128;
        memcpy(gl_profile.service_id.id.uuid.uuid.uuid128, gattsServiceCo2Uuid, sizeof(gattsServiceCo2Uuid));

        esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name(test_device_name);
        if (set_dev_name_ret)
        {
            ESP_LOGE(TAG, "set device name failed, error code = %x", set_dev_name_ret);
        }
        // config adv data
        esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
        if (ret)
        {
            ESP_LOGE(TAG, "config adv data failed, error code = %x", ret);
        }
        adv_config_done |= adv_config_flag;
        // config scan response data
        ret = esp_ble_gap_config_adv_data(&scan_rsp_data);
        if (ret)
        {
            ESP_LOGE(TAG, "config scan response data failed, error code = %x", ret);
        }
        adv_config_done |= scan_rsp_config_flag;

        esp_ble_gatts_create_service(gatts_if, &gl_profile.service_id, GATTS_NUM_HANDLE_TEST_A);
        break;
    case ESP_GATTS_READ_EVT:
    {
        ESP_LOGI(TAG,
                 "Characteristic read request: conn_id=%d, trans_id=%" PRIu32 ", handle=%d, is_long=%d, offset=%d, need_rsp=%d",
                 param->read.conn_id, param->read.trans_id, param->read.handle,
                 param->read.is_long, param->read.offset, param->read.need_rsp);

        // If no response is needed, exit early (stack handles it automatically)
        if (!param->read.need_rsp)
        {
            return;
        }

        esp_gatt_rsp_t rsp;
        memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
        rsp.attr_value.handle = param->read.handle;

        // Handle descriptor read request
        if (param->read.handle == gl_profile.descr_handle)
        {
            memcpy(rsp.attr_value.value, &descr_value, 2);
            rsp.attr_value.len = 2;
            esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
            return;
        }

        // Handle characteristic read request
        if (param->read.handle == gl_profile.char_handle)
        {
            uint16_t offset = param->read.offset;

            // Validate read offset
            if (param->read.is_long && offset > CONFIG_EXAMPLE_CHAR_READ_DATA_LEN)
            {
                ESP_LOGW(TAG, "Read offset (%d) out of range (0-%d)", offset, CONFIG_EXAMPLE_CHAR_READ_DATA_LEN);
                rsp.attr_value.len = 0;
                esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_INVALID_OFFSET, &rsp);
                return;
            }

            // Determine response length based on MTU
            uint16_t mtu_size = local_mtu - 1; // ATT header (1 byte)
            uint16_t send_len = (CONFIG_EXAMPLE_CHAR_READ_DATA_LEN - offset > mtu_size) ? mtu_size : (CONFIG_EXAMPLE_CHAR_READ_DATA_LEN - offset);

            memcpy(rsp.attr_value.value, &char_value_read[offset], send_len);
            rsp.attr_value.len = send_len;

            // Send response to GATT client
            esp_err_t err = esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to send response: %s", esp_err_to_name(err));
            }
        }
        break;
    }
    case ESP_GATTS_WRITE_EVT:
    {
        ESP_LOGI(TAG, "Characteristic write, conn_id %d, trans_id %" PRIu32 ", handle %d", param->write.conn_id, param->write.trans_id, param->write.handle);
        if (!param->write.is_prep)
        {
            ESP_LOGI(TAG, "value len %d, value ", param->write.len);
            ESP_LOG_BUFFER_HEX(TAG, param->write.value, param->write.len);
            if (gl_profile.descr_handle == param->write.handle && param->write.len == 2)
            {
                descr_value = param->write.value[1] << 8 | param->write.value[0];
                if (descr_value == 0x0001)
                {
                    if (a_property & ESP_GATT_CHAR_PROP_BIT_NOTIFY)
                    {
                        ESP_LOGI(TAG, "Notification enable");
                        uint8_t notify_data[15];
                        for (int i = 0; i < sizeof(notify_data); ++i)
                        {
                            notify_data[i] = i % 0xff;
                        }
                        // the size of notify_data[] need less than MTU size
                        esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, gl_profile.char_handle,
                                                    sizeof(notify_data), notify_data, false);
                    }
                }
                else if (descr_value == 0x0002)
                {
                    if (a_property & ESP_GATT_CHAR_PROP_BIT_INDICATE)
                    {
                        ESP_LOGI(TAG, "Indication enable");
                        uint8_t indicate_data[15];
                        for (int i = 0; i < sizeof(indicate_data); ++i)
                        {
                            indicate_data[i] = i % 0xff;
                        }
                        // the size of indicate_data[] need less than MTU size
                        esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, gl_profile.char_handle,
                                                    sizeof(indicate_data), indicate_data, true);
                    }
                }
                else if (descr_value == 0x0000)
                {
                    ESP_LOGI(TAG, "Notification/Indication disable");
                }
                else
                {
                    ESP_LOGE(TAG, "Unknown descriptor value");
                    ESP_LOG_BUFFER_HEX(TAG, param->write.value, param->write.len);
                }
            }
        }
        example_write_event_env(gatts_if, &a_prepare_write_env, param);
        break;
    }
    case ESP_GATTS_EXEC_WRITE_EVT:
        ESP_LOGI(TAG, "Execute write");
        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
        example_exec_write_event_env(&a_prepare_write_env, param);
        break;
    case ESP_GATTS_MTU_EVT:
        ESP_LOGI(TAG, "MTU exchange, MTU %d", param->mtu.mtu);
        local_mtu = param->mtu.mtu;
        break;
    case ESP_GATTS_UNREG_EVT:
        break;
    case ESP_GATTS_CREATE_EVT:
        ESP_LOGI(TAG, "Service create, status %d, service_handle %d", param->create.status, param->create.service_handle);
        gl_profile.service_handle = param->create.service_handle;
        gl_profile.char_uuid.len = ESP_UUID_LEN_16;
        gl_profile.char_uuid.uuid.uuid16 = GATTS_CHAR_UUID_TEST_A;

        esp_ble_gatts_start_service(gl_profile.service_handle);
        a_property = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
        esp_err_t add_char_ret = esp_ble_gatts_add_char(gl_profile.service_handle, &gl_profile.char_uuid,
                                                        ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                                        a_property,
                                                        &gatts_demo_char1_val, NULL);
        if (add_char_ret)
        {
            ESP_LOGE(TAG, "add char failed, error code =%x", add_char_ret);
        }
        break;
    case ESP_GATTS_ADD_INCL_SRVC_EVT:
        break;
    case ESP_GATTS_ADD_CHAR_EVT:
    {
        uint16_t length = 0;
        const uint8_t *prf_char;

        ESP_LOGI(TAG, "Characteristic add, status %d, attr_handle %d, service_handle %d",
                 param->add_char.status, param->add_char.attr_handle, param->add_char.service_handle);
        gl_profile.char_handle = param->add_char.attr_handle;
        gl_profile.descr_uuid.len = ESP_UUID_LEN_16;
        gl_profile.descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_DESCRIPTION;
        esp_err_t get_attr_ret = esp_ble_gatts_get_attr_value(param->add_char.attr_handle, &length, &prf_char);
        if (get_attr_ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "ILLEGAL HANDLE");
        }

        ESP_LOGI(TAG, "the gatts demo char length = %x", length);
        for (int i = 0; i < length; i++)
        {
            ESP_LOGI(TAG, "prf_char[%x] =%x", i, prf_char[i]);
        }
        esp_err_t add_descr_ret = esp_ble_gatts_add_char_descr(gl_profile.service_handle, &gl_profile.descr_uuid,
                                                               ESP_GATT_PERM_READ, NULL, NULL);
        if (add_descr_ret)
        {
            ESP_LOGE(TAG, "add char descr failed, error code =%x", add_descr_ret);
        }
        break;
    }
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        gl_profile.descr_handle = param->add_char_descr.attr_handle;
        ESP_LOGI(TAG, "Descriptor add, status %d, attr_handle %d, service_handle %d",
                 param->add_char_descr.status, param->add_char_descr.attr_handle, param->add_char_descr.service_handle);
        break;
    case ESP_GATTS_DELETE_EVT:
        break;
    case ESP_GATTS_START_EVT:
        ESP_LOGI(TAG, "Service start, status %d, service_handle %d",
                 param->start.status, param->start.service_handle);
        break;
    case ESP_GATTS_STOP_EVT:
        break;
    case ESP_GATTS_CONNECT_EVT:
    {
        esp_ble_conn_update_params_t conn_params = {0};
        memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        /* For the IOS system, please reference the apple official documents about the ble connection parameters restrictions. */
        conn_params.latency = 0;
        conn_params.max_int = 0x20; // max_int = 0x20*1.25ms = 40ms
        conn_params.min_int = 0x10; // min_int = 0x10*1.25ms = 20ms
        conn_params.timeout = 400;  // timeout = 400*10ms = 4000ms
        ESP_LOGI(TAG, "Connected, conn_id %u, remote " ESP_BD_ADDR_STR "",
                 param->connect.conn_id, ESP_BD_ADDR_HEX(param->connect.remote_bda));
        gl_profile.conn_id = param->connect.conn_id;
        // start sent the update connection parameters to the peer device.
        esp_ble_gap_update_conn_params(&conn_params);
        break;
    }
    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(TAG, "Disconnected, remote " ESP_BD_ADDR_STR ", reason 0x%02x",
                 ESP_BD_ADDR_HEX(param->disconnect.remote_bda), param->disconnect.reason);
        esp_ble_gap_start_advertising(&adv_params);
        local_mtu = 23; // Reset MTU for a single connection
        break;
    case ESP_GATTS_CONF_EVT:
        ESP_LOGI(TAG, "Confirm receive, status %d, attr_handle %d", param->conf.status, param->conf.handle);
        if (param->conf.status != ESP_GATT_OK)
        {
            ESP_LOG_BUFFER_HEX(TAG, param->conf.value, param->conf.len);
        }
        break;
    case ESP_GATTS_OPEN_EVT:
    case ESP_GATTS_CANCEL_OPEN_EVT:
    case ESP_GATTS_CLOSE_EVT:
    case ESP_GATTS_LISTEN_EVT:
    case ESP_GATTS_CONGEST_EVT:
    default:
        break;
    }
}

static void GattsEventHandler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    /* If event is register event, store the gatts_if for each profile */
    if (event == ESP_GATTS_REG_EVT)
    {
        if (param->reg.status == ESP_GATT_OK)
        {
            gl_profile.gatts_if = gatts_if;
        }
        else
        {
            ESP_LOGI(TAG, "Reg app failed, app_id %04x, status %d",
                     param->reg.app_id,
                     param->reg.status);
            return;
        }
    }

    /* If the gatts_if equal to profile A, call profile A cb handler,
     * so here call each profile's callback */
    do
    {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++)
        {
            if (gatts_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
                gatts_if == gl_profile.gatts_if)
            {
                if (gl_profile.gatts_cb)
                {
                    gl_profile.gatts_cb(event, gatts_if, param);
                }
            }
        }
    } while (0);
}