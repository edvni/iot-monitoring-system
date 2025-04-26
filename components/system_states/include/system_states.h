#ifndef SYSTEM_STATES_H
#define SYSTEM_STATES_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

// System states enum
typedef enum {
    STATE_NORMAL = 0,
    STATE_FIRST_BLOCK_RECOVERY = 1,
    STATE_SECOND_BLOCK_RECOVERY = 2, 
    STATE_THIRD_BLOCK_RECOVERY = 3
} system_state_t;

/**
 * @brief Handles unsuccessful initialization
 * 
 * This function logs an error, sets the error flag, powers off modem, 
 * waits for 10 seconds, and then restarts the system.
 */
void unsuccessful_init(const char *tag);

/**
 * @brief Set the system state
 * 
 * @param state System state
 * @return esp_err_t ESP_OK on success
 */
esp_err_t set_system_state(system_state_t state);

/**
 * @brief Get the system state
 * 
 * @return system_state_t System state
 */
system_state_t get_system_state(void);

/**
 * @brief Check if this is the first boot ever
 * 
 * @return bool true if this is the first boot
 */
bool is_first_boot(void);

/**
 * @brief Mark that first boot has been completed
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mark_first_boot_completed(void);

/**
 * @brief Initialize system state module
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t system_state_init(void);

/**
 * @brief Get the boot count
 * 
 * @return uint32_t Boot count
 */
uint32_t get_boot_count(void);

/**
 * @brief Increment the boot count
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t increment_boot_count(void);

/**
 * @brief Reset the boot counter to 0
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t reset_boot_counter(void);

/**
 * @brief Set error flag
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t set_error_flag(void);

/**
 * @brief Get error flag
 * 
 * @return bool True if error occurred in previous cycle
 */
bool get_error_flag(void);

#endif /* SYSTEM_STATES_H */