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
 * @return esp_err_t ESP_OK on success
 */
esp_err_t sensors_init(void);

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

/**
 * @brief Get the number of sensors from which data has been received
 * 
 * @return int Number of sensors with data
 */
int sensors_get_received_count(void);

/**
 * @brief Check if any sensor data has been received
 * 
 * @return bool true if data has been received from at least one sensor
 */
bool sensors_any_data_received(void);

/**
 * @brief Get the total number of configured sensors
 * 
 * @return int Number of configured sensors
 */
int sensors_get_total_count(void);

/**
 * @brief Reset the status of all sensors
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t sensors_reset_status(void);

/**
 * @brief Сброс флага получения данных
 */
void sensors_reset_data_received_flag(void);

/**
 * @brief Проверка, были ли получены какие-либо данные
 * 
 * @return true если данные были получены
 */
bool sensors_is_data_received(void);

/**
 * @brief Устанавливает флаг получения данных
 */
void sensors_set_data_received(void);