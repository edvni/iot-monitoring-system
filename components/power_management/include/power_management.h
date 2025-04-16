#ifndef POWER_MANAGEMENT_H
#define POWER_MANAGEMENT_H

#include "esp_err.h"
/**
 * @brief Initialize power management
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t power_management_init(void);

#endif // POWER_MANAGEMENT_H