#include "sensors.h"
#include <string.h>
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "storage.h"

static const char *TAG = "SENSORS";

/**
 * @brief Array of MAC addresses of allowed sensors
 * 
 * To add a new sensor:
 * 1. Add the MAC address to this array
 * 2. The system will automatically:
 *    - Track data only from these sensors
 *    - Save data in separate files for each sensor
 *    - Send all data to Firebase with unique document_id
 * 
 * Example format of MAC addresses: "XX:XX:XX:XX:XX:XX" (all uppercase letters)
 */
static const char *TARGET_MACS[] = {
    "DB:C3:58:D9:03:70", // Vladimir RuuviTag
    "EE:C4:12:FD:CA:DF" // Vladimir RuuviTag
    //"C4:D9:12:ED:63:C6", // Edvard RuuviTag
    //"C5:71:D4:30:42:34" // Johannes RuuviTag
};
#define MAX_SENSORS (sizeof(TARGET_MACS) / sizeof(TARGET_MACS[0]))

// Structure for tracking sensor status
typedef struct {
    char mac_address[18];     // MAC address of the sensor
    bool data_received;       // Flag for data received
    uint64_t last_timestamp;  // Timestamp of the last data received
} sensor_status_t;

static sensor_status_t sensor_status[MAX_SENSORS];
static int sensors_received_count = 0;
static bool any_data_received = false;
static volatile bool s_data_received = false; // Флаг получения данных
static ruuvi_callback_t measurement_callback = NULL;

// Ruuvi manufacturer specific data
static const uint16_t RUUVI_COMPANY_ID = 0x0499;
static const uint8_t RUUVI_RAW_V2 = 0x05;

// NimBLE scan parameters
static struct ble_gap_disc_params scan_params = {
    .itvl = BLE_GAP_SCAN_ITVL_MS(100),      // 100ms scan interval
    .window = BLE_GAP_SCAN_WIN_MS(75),      // 75ms scan window 
    .filter_policy = 0,                     // No filter policy
    .limited = 0,                           // Not limited discovery
    .passive = 0,                           // Active scanning for better detection
    .filter_duplicates = 0                  // Do not filter duplicates
};

// BLE scan parameters
static void ble_app_on_sync(void);
static void ble_host_task(void *param);

// Initialize sensor status
static void init_sensor_status(void) {
    for (int i = 0; i < MAX_SENSORS; i++) {
        strncpy(sensor_status[i].mac_address, TARGET_MACS[i], sizeof(sensor_status[i].mac_address));
        sensor_status[i].data_received = false;
        sensor_status[i].last_timestamp = 0;
    }
    sensors_received_count = 0;
    any_data_received = false;
}

// Reset all sensors status
esp_err_t sensors_reset_status(void) {
    ESP_LOGI(TAG, "Resetting sensor status data");
    
    // Reset all sensors status
    for (int i = 0; i < MAX_SENSORS; i++) {
        sensor_status[i].data_received = false;
        sensor_status[i].last_timestamp = 0;
    }
    
    sensors_received_count = 0;
    any_data_received = false;
    
    return ESP_OK;
}

// Check if data is received from a specific sensor
static bool is_data_received_from_sensor(const char* mac_address) {
    for (int i = 0; i < MAX_SENSORS; i++) {
        if (strcmp(sensor_status[i].mac_address, mac_address) == 0) {
            return sensor_status[i].data_received;
        }
    }
    return false;
}

// Update sensor status when data is received
static void update_sensor_received(const char* mac_address, uint64_t timestamp) {
    for (int i = 0; i < MAX_SENSORS; i++) {
        if (strcmp(sensor_status[i].mac_address, mac_address) == 0) {
            if (!sensor_status[i].data_received) {
                sensor_status[i].data_received = true;
                sensor_status[i].last_timestamp = timestamp;
                sensors_received_count++;
                any_data_received = true;
                ESP_LOGI(TAG, "Received data from sensor %d (%s), total sensors received: %d/%d", 
                         i, mac_address, sensors_received_count, (int)MAX_SENSORS);
            }
            return;
        }
    }
}

// Get the number of sensors from which data has been received
int sensors_get_received_count(void) {
    return sensors_received_count;
}

// Check if data has been received from at least one sensor
bool sensors_any_data_received(void) {
    return any_data_received;
}

// Get the total number of configured sensors
int sensors_get_total_count(void) {
    return MAX_SENSORS;
}

static void parse_ruuvi_data(const uint8_t *data, uint8_t len, ruuvi_measurement_t *measurement) {
    if (len < 14 || data[0] != RUUVI_RAW_V2) {
        return;
    }

    // Temperature in 0.005 degrees
    int16_t temp = (data[1] << 8) + data[2];
    measurement->temperature = (float)temp * 0.005;

    // Humidity in 0.0025%
    uint16_t humidity = (data[3] << 8) + data[4];
    measurement->humidity = (float)humidity * 0.0025;
}

static int ble_gap_event(struct ble_gap_event *event, void *arg) {
    struct ble_hs_adv_fields fields;

    switch (event->type) {
        case BLE_GAP_EVENT_DISC:
            // Try to parse advertisement data
            if (ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data) == 0) {
                // Check for manufacturer specific data
                if (fields.mfg_data_len > 0) {
                    uint16_t company_id = fields.mfg_data[0] | (fields.mfg_data[1] << 8);
                    if (company_id == RUUVI_COMPANY_ID && measurement_callback) {
                        ruuvi_measurement_t measurement = {0};
                        
                        // Format MAC address
                        sprintf(measurement.mac_address, "%02X:%02X:%02X:%02X:%02X:%02X",
                                event->disc.addr.val[5], event->disc.addr.val[4],
                                event->disc.addr.val[3], event->disc.addr.val[2],
                                event->disc.addr.val[1], event->disc.addr.val[0]);
                        
                        // Check if this is one of our target RuuviTags
                        bool is_target = false;
                        for (int i = 0; i < MAX_SENSORS; i++) {
                            if (strcmp(measurement.mac_address, TARGET_MACS[i]) == 0) {
                                is_target = true;
                                break;
                            }
                        }
                        
                        if (is_target) {
                            // Check if data has already been received from this sensor
                            if (is_data_received_from_sensor(measurement.mac_address)) {
                                ESP_LOGD(TAG, "Already received data from %s in this cycle", measurement.mac_address);
                                return 0;
                            }

                            // Parse measurement data
                            parse_ruuvi_data(fields.mfg_data + 2, fields.mfg_data_len - 2, &measurement);
                            
                            // Set timestamp
                            measurement.timestamp = esp_timer_get_time() / 1000000;
                            
                            // Update sensor status
                            update_sensor_received(measurement.mac_address, measurement.timestamp);
                            
                            // Call user callback
                            measurement_callback(&measurement);
                        }
                    }
                }
            }
            break;

        default:
            break;
    }
    return 0;
}

// Внутренний коллбэк для обработки данных от RuuviTag
static void internal_ruuvi_data_callback(ruuvi_measurement_t *measurement) {
    esp_err_t ret = storage_save_measurement(measurement);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save measurement from %s", measurement->mac_address);
    } else {
        ESP_LOGI(TAG, "Successfully saved data from %s", measurement->mac_address);
        s_data_received = true; // Устанавливаем внутренний флаг
    }
}

// Новые функции для управления флагом получения данных
void sensors_reset_data_received_flag(void) {
    s_data_received = false;
}

bool sensors_is_data_received(void) {
    return s_data_received;
}

void sensors_set_data_received(void) {
    s_data_received = true;
}

// Изменённая функция инициализации
esp_err_t sensors_init(void) {
    int rc;

    // Используем внутренний коллбэк
    measurement_callback = internal_ruuvi_data_callback;
    
    // Initialize sensor status
    init_sensor_status();

    // Initialize NimBLE host stack
    ESP_LOGI(TAG, "Initializing NimBLE host stack");
    rc = nimble_port_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to init NimBLE port; rc=%d", rc);
        return ESP_FAIL;
    }

    // Initialize NimBLE host configuration
    ble_hs_cfg.sync_cb = ble_app_on_sync;
    
    // Set initial security capabilities
    ble_hs_cfg.sm_sc = 0;  // Disable secure connections
    
    // Initialize the NimBLE host task
    nimble_port_freertos_init(ble_host_task);
    
    ESP_LOGI(TAG, "Scanning for %d configured sensors", (int)MAX_SENSORS);
    for (int i = 0; i < MAX_SENSORS; i++) {
        ESP_LOGI(TAG, "Sensor %d MAC: %s", i, TARGET_MACS[i]);
    }
    
    return ESP_OK;
}

// Callback when host and controller are in sync
static void ble_app_on_sync(void) {
    int rc;

    // Enable BLE scanner with defined parameters
    rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, BLE_HS_FOREVER, &scan_params,
                      ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error initiating GAP discovery procedure; rc=%d", rc);
    }
}

// The NimBLE host task
static void ble_host_task(void *param) {
    ESP_LOGI(TAG, "BLE Host Task Started");
    nimble_port_run(); // This function will return only when nimble_port_stop() is executed
    nimble_port_freertos_deinit();
}

esp_err_t sensors_start_scan(uint32_t duration_sec) {
    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, 
                         duration_sec ? (duration_sec * 1000) : BLE_HS_FOREVER,
                         &scan_params,
                         ble_gap_event, 
                         NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error starting GAP discovery procedure; rc=%d", rc);
        return ESP_FAIL;
    }
    return ESP_OK;
 }
 
esp_err_t sensors_stop_scan(void) {
    int rc = ble_gap_disc_cancel();
    if (rc != 0) {
        ESP_LOGE(TAG, "Error canceling GAP discovery procedure; rc=%d", rc);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t sensors_deinit(void) {
    // Only stop scanning //
    int rc = ble_gap_disc_cancel();
    ESP_LOGW(TAG, "Canceling GAP discovery procedure; rc=%d", rc);
    
    // Clearing callback
    measurement_callback = NULL;
    
    // Do not call nimble_port_stop() and nimble_port_deinit()
    // This will prevent Guru Meditation Error, but may lead to resource leaks
    
    return ESP_OK;
}