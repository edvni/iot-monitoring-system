#include <stdio.h>
#include <inttypes.h>  
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
#include "nvs_flash.h"
#include "sensors.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"
#include "esp_mac.h"


#define TIME_TO_SLEEP    600        // 10 minutes in seconds
#define SECONDS_PER_DAY  86400      // 24 hours * 60 minutes * 60 seconds
#define uS_TO_S_FACTOR   1000000ULL // Conversion factor for micro seconds to seconds


// Variable in RTC memory for time counting
RTC_DATA_ATTR static uint32_t boot_count = 0;

static const char *TAG = "MAIN";

// Callback function for receiving data from RuuviTag
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

    // Increment boot counter
    boot_count++;
    ESP_LOGI(TAG, "Boot count: %" PRIu32, boot_count); 

    // Check if 24 hours have passed
    if (boot_count * TIME_TO_SLEEP >= SECONDS_PER_DAY) {
        ESP_LOGI(TAG, "24 hours passed, resetting counter");
        boot_count = 0;
        // TODO: Reset counter in RTC memory
    }

    // Initialize BLE scanner for RuuviTag
    ESP_LOGI(TAG, "Initializing RuuviTag scanner...");
    ESP_ERROR_CHECK(sensors_init(ruuvi_data_callback));

    // Give some time for scanning (e.g. 15 seconds)
    vTaskDelay(pdMS_TO_TICKS(5000));

    // Deinitialize BLE before going to sleep
    sensors_deinit();

    // Set timer to wake up after TIME_TO_SLEEP seconds
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    ESP_LOGI(TAG, "Going to sleep for %d seconds", TIME_TO_SLEEP);

    // Deep sleep
    esp_deep_sleep_start();
}