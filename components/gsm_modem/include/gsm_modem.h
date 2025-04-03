#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Structure for battery status
 */
typedef struct {
    int voltage;        // Voltage in mV
    int charge_status;  // -1: Unknown, 0: Not charging, 1: Charging, 2: Charging complete
    int level;          // Charge level in percentage (-1 if unavailable)
} battery_status_t;



/**
 * @brief Initialize GSM modem
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t gsm_modem_init(void);


/**
 * @brief Deinitialization of the GSM modem
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t gsm_modem_deinit(void);

/**
 * @brief Getting battery charge
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t gsm_modem_get_battery_status(battery_status_t* status);

/**
 * @brief Get current time from GSM network
 * 
 * @return time_t Unix timestamp on success, -1 on failure
 */
time_t gsm_get_network_time(void);


/**
 * @brief Power on the modem
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t modem_power_off(void);

#ifdef __cplusplus
}
#endif