#ifndef STORAGE_H
#define STORAGE_H

#include "esp_err.h"
#include <stdbool.h>
#include "sensors.h"
#include <stdint.h>
#include "system_states.h"


#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Initialize NVS storage
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t storage_init(void);

/**
 * @brief Save the sensor measurement to SPIFFS
 * 
 * This function implements the mechanism for saving data for working with multiple sensors:
 * 1. Creates a separate file for each sensor based on the MAC address
 * 2. Files are named as "/spiffs/sensor_XX_XX_XX_XX_XX_XX.json"
 * 3. The function storage_get_sensor_files() will later find these files 
 *    for sending to Firebase
 * 
 * @param measurement Structure with measurement data
 * @return esp_err_t ESP_OK on success
 */
esp_err_t storage_save_measurement(ruuvi_measurement_t *measurement);

/**
 * @brief Append a log message to the log file
 * 
 * @param log_message Log message
 * @return esp_err_t ESP_OK on success
 */
esp_err_t storage_append_log(const char* log_message);



/**
 * @brief Get the logs from the log file
 * 
 * @return char* Logs
 */
char* storage_get_logs(void);

/**
 * @brief Get a list of all sensor files in SPIFFS
 * 
 * @param file_list Pointer to array of strings that will be filled with file paths
 * @param file_count Number of files found
 * @return esp_err_t ESP_OK on success
 */
esp_err_t storage_get_sensor_files(char ***file_list, int *file_count);

/**
 * @brief Free the memory allocated for the sensor file list
 * 
 * @param file_list Array of strings containing file paths
 * @param file_count Number of files in the list
 */
void storage_free_sensor_files(char **file_list, int file_count);

/**
 * @brief Synchronize the file system to ensure all data is written to flash
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t storage_sync(void);

#ifdef __cplusplus
}
#endif

#endif 