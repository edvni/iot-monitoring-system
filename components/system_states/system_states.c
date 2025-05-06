// components/system_states/system_states.c
#include "system_states.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "storage.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gsm_modem.h"
#include "esp_system.h"
#include "config_manager.h"

#define FIRST_BOOT_KEY "first_boot"

static const char *TAG = "SYSTEM_STATE";
static nvs_handle_t state_nvs_handle;

// Initialize the system state storage
esp_err_t system_state_init(void) {
    esp_err_t ret = nvs_open("system_state", NVS_READWRITE, &state_nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle for system state: %s", esp_err_to_name(ret));
    }
    return ret;
}

// Restart the system after unsuccessful initialization
void unsuccessful_init(const char *tag) {
    ESP_LOGE(tag, "Unsuccessful initialization detected");
    storage_append_log("Unsuccessful initialization detected");
    ESP_LOGE(tag, "Restarting modem in 10 seconds");
    set_error_flag();
    modem_power_off();
    vTaskDelay(pdMS_TO_TICKS(10000)); // Restart in 10 seconds
    esp_restart();
}

// Set the system state
esp_err_t set_system_state(system_state_t state) {
    esp_err_t ret = nvs_set_u8(state_nvs_handle, "system_state", (uint8_t)state);
    if (ret != ESP_OK) return ret;
    return nvs_commit(state_nvs_handle);
}

// Get the system state
system_state_t get_system_state(void) {
    uint8_t state = STATE_NORMAL;
    esp_err_t ret = nvs_get_u8(state_nvs_handle, "system_state", &state);
    return (ret == ESP_OK) ? (system_state_t)state : STATE_NORMAL;
}

// Check if this is the first boot ever
bool is_first_boot(void) {
    uint8_t first_boot = 0;
    esp_err_t ret = nvs_get_u8(state_nvs_handle, FIRST_BOOT_KEY, &first_boot);
    return (ret == ESP_ERR_NVS_NOT_FOUND || first_boot == 0);
}

// Mark that first boot has been completed
esp_err_t mark_first_boot_completed(void) {
    esp_err_t ret = nvs_set_u8(state_nvs_handle, FIRST_BOOT_KEY, 1);
    return (ret != ESP_OK) ? ret : nvs_commit(state_nvs_handle);
}

// Get the boot count
uint32_t get_boot_count(void) {
   uint32_t boot_count = 0;
   if (nvs_get_u32(state_nvs_handle, "boot_count", &boot_count) == ESP_ERR_NVS_NOT_FOUND) {
       ESP_LOGI(TAG, "Boot count not found, starting from 0");
   }
   return boot_count;
}

// Increment the boot count
esp_err_t increment_boot_count(void) {
   uint32_t boot_count = get_boot_count() + 1;
   esp_err_t ret = nvs_set_u32(state_nvs_handle, "boot_count", boot_count);
   return (ret != ESP_OK) ? ret : nvs_commit(state_nvs_handle);
}

// Reset the boot counter to 0
esp_err_t reset_boot_counter(void) {
   esp_err_t ret = nvs_set_u32(state_nvs_handle, "boot_count", 0);
   return (ret != ESP_OK) ? ret : nvs_commit(state_nvs_handle);
}

// Set the error flag
esp_err_t set_error_flag(void) {
    esp_err_t ret = nvs_set_u8(state_nvs_handle, "error_flag", 1);
    return (ret != ESP_OK) ? ret : nvs_commit(state_nvs_handle);
}

// Get the error flag
bool get_error_flag(void) {
    uint8_t flag = 0;
    esp_err_t ret = nvs_get_u8(state_nvs_handle, "error_flag", &flag);
    return (ret == ESP_OK && flag == 1);
}