#ifndef STORAGE_H
#define STORAGE_H

#include "esp_err.h"
#include "sensors.h"
#include <stdint.h>


// Initialize NVS
esp_err_t storage_init(void);

// Get the boot count from NVS
uint32_t storage_get_boot_count(void);

// Increment the boot count and save it to NVS
esp_err_t storage_increment_boot_count(void);

// Check if 24 hours passed and reset the counter
esp_err_t storage_check_and_reset_counter(uint32_t sleep_time);

// Save a measurement to SPIFFS
esp_err_t storage_save_measurement(ruuvi_measurement_t *measurement);

// Get the measurements from SPIFFS
char* storage_get_measurements(void);

// Clear the measurements from SPIFFS
esp_err_t storage_clear_measurements(void);

/**
 * @brief Check if this is the first boot of the device
 * @return true if first boot, false otherwise
 */
bool storage_is_first_boot(void);

/**
 * @brief Mark first boot as completed
 * @return ESP_OK on success
 */
esp_err_t storage_set_first_boot_completed(void);

#endif // STORAGE_H