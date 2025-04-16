#pragma once

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief RuuviTag measurement data structure
 */
typedef struct {
    char mac_address[18];    // MAC address as string "XX:XX:XX:XX:XX:XX"
    float temperature;       // Temperature in Celsius
    float humidity;         // Relative humidity percentage
    uint64_t timestamp;     // UNIX timestamp of measurement
} ruuvi_measurement_t;

/**
 * @brief Callback function type for receiving RuuviTag measurements
 */
typedef void (*ruuvi_callback_t)(ruuvi_measurement_t *measurement);

/**
 * @brief Initialize BLE scanner for RuuviTag
 * 
 * @param callback Function to be called when measurement is received
 * @return esp_err_t ESP_OK on success
 */
esp_err_t sensors_init(ruuvi_callback_t callback);

/**
 * @brief Start BLE scanning
 * 
 * @param duration_sec Duration in seconds, 0 for continuous scanning
 * @return esp_err_t ESP_OK on success
 */
esp_err_t sensors_start_scan(uint32_t duration_sec);

/**
 * @brief Stop BLE scanning
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t sensors_stop_scan(void);

/**
 * @brief Deinitialize BLE scanner
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t sensors_deinit(void);