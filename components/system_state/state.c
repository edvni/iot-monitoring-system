#include "state.h"
#include "storage.h"
#include "esp_log.h"
#include "esp_system.h"
#include "gsm_modem.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


// Unsuccessful initialization
void unsuccessful_init(const char *TAG) {
    //ESP_LOGE(TAG, "Failed to initialize GSM or Discord API");
    storage_append_log("Unsuccessful initialization detected");
    ESP_LOGE(TAG, "Restarting modem in 10 seconds");
    storage_set_error_flag();
    modem_power_off();
    vTaskDelay(pdMS_TO_TICKS(10000)); // Restart in 10 seconds
    esp_restart();
}