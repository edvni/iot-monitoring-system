#ifndef STORAGE_H
#define STORAGE_H

#include "esp_err.h"
#include <stdbool.h>
#include "sensors.h"
#include <stdint.h>
#include "system_state.h"


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
 * @brief Get the boot count from NVS
 * 
 * @return uint32_t Boot count
 */
uint32_t storage_get_boot_count(void);


/**
 * @brief Increment the boot count and save it to NVS
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t storage_increment_boot_count(void);


/**
 * @brief Save a measurement to SPIFFS
 * 
 * @param measurement Measurement to save
 * @return esp_err_t ESP_OK on success
 */
esp_err_t storage_save_measurement(ruuvi_measurement_t *measurement);


/**
 * @brief Get the measurements from SPIFFS
 * 
 * @return char* Measurements in JSON format
 * 
 */
char* storage_get_measurements(void);


/**
 * @brief Clear the measurements from SPIFFS
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t storage_clear_measurements(void);



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
 * @brief Reset the boot counter
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t storage_reset_counter(void);


/**
 * @brief Set the error flag if error occurred in last cycle
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t storage_set_error_flag(void);


/**
 * @brief Get the error flag
 * 
 * @return bool Error flag
 */
bool storage_get_error_flag(void);


/**
 * @brief Clear the error flag
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t storage_clear_error_flag(void);


/**
 * @brief Set the system state
 * 
 * @param state System state
 * @return esp_err_t ESP_OK on success
 */
esp_err_t storage_set_system_state(system_state_t state);


/**
 * @brief Get the system state
 * 
 * @return system_state_t System state
 */
system_state_t storage_get_system_state(void);


#ifdef __cplusplus
}
#endif

#endif /* STORAGE_H */
