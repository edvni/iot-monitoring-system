#include <stdio.h>
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
#include "nvs_flash.h"
#include "sensors.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MAIN";

// Callback функция для получения данных от RuuviTag
static void ruuvi_data_callback(ruuvi_measurement_t *measurement) {
    ESP_LOGI(TAG, "RuuviTag data:");
    ESP_LOGI(TAG, "  MAC: %s", measurement->mac_address);
    ESP_LOGI(TAG, "  Temperature: %.2f °C", measurement->temperature);
    ESP_LOGI(TAG, "  Humidity: %.2f %%", measurement->humidity);
    ESP_LOGI(TAG, "  Timestamp: %llu", measurement->timestamp);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Инициализация сканера RuuviTag
    ESP_LOGI(TAG, "Initializing RuuviTag scanner...");
    ESP_ERROR_CHECK(sensors_init(ruuvi_data_callback));

    ESP_LOGI(TAG, "Scanner running...");
}