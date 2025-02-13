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

static const char *TAG = "SENSORS";
static const char *TARGET_MAC = "DB:C3:58:D9:03:70";
static ruuvi_callback_t measurement_callback = NULL;

// Ruuvi manufacturer specific data
static const uint16_t RUUVI_COMPANY_ID = 0x0499;
static const uint8_t RUUVI_RAW_V2 = 0x05;

// NimBLE scan parameters
static struct ble_gap_disc_params scan_params = {
    .itvl = BLE_GAP_SCAN_ITVL_MS(100),      // 100ms scan interval
    .window = BLE_GAP_SCAN_WIN_MS(50),       // 50ms scan window
    .filter_policy = 0,                      // No filter policy
    .limited = 0,                            // Not limited discovery
    .passive = 1,                            // Passive scanning
    .filter_duplicates = 0                   // Don't filter duplicates
};

// BLE scan parameters
static void ble_app_on_sync(void);
static void ble_host_task(void *param);

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
                        
                        // Parse measurement data (manufacturer specific data without company id)
                        parse_ruuvi_data(fields.mfg_data + 2, fields.mfg_data_len - 2, &measurement);
                        
                        // Set timestamp
                        measurement.timestamp = esp_timer_get_time() / 1000000; // Convert to seconds
                        
                        // Call user callback
                        measurement_callback(&measurement);
                    }
                }
            }
            break;

        default:
            break;
    }
    return 0;
}

esp_err_t sensors_init(ruuvi_callback_t callback) {
    int rc;

    if (callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Store callback
    measurement_callback = callback;

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
    int rc;
 
    // Stop scanning if active
    sensors_stop_scan();
 
    // Stop the NimBLE host task
    rc = nimble_port_stop();
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to stop nimble port; rc=%d", rc);
        return ESP_FAIL;
    }
 
    // Deinitialize NimBLE
    nimble_port_deinit();
    
    measurement_callback = NULL;
    return ESP_OK;
 }