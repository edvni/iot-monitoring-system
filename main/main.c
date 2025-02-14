#include <stdio.h>
#include <inttypes.h>  
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
#include "nvs_flash.h"
#include "sensors.h"
#include "storage.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"
#include "esp_mac.h"
#include "esp_pm.h"

#define TIME_TO_SLEEP    600        // Time in seconds to go to sleep        
#define SECONDS_PER_DAY  86400      // 24 hours in seconds
#define uS_TO_S_FACTOR   1000000ULL // Conversion factor for micro seconds to seconds

static const char *TAG = "MAIN";

static volatile bool data_received = false;  // Flag for data received

static void ruuvi_data_callback(ruuvi_measurement_t *measurement) {
    ESP_LOGI(TAG, "RuuviTag data:");
    ESP_LOGI(TAG, "  MAC: %s", measurement->mac_address);
    ESP_LOGI(TAG, "  Temperature: %.2f °C", measurement->temperature);
    ESP_LOGI(TAG, "  Humidity: %.2f %%", measurement->humidity);
    ESP_LOGI(TAG, "  Timestamp: %llu", measurement->timestamp);

    data_received = true; // Set the flag
}

void app_main(void)
{   // Storage initialization  
    ESP_ERROR_CHECK(storage_init());

    // Increment and save the counter
    ESP_ERROR_CHECK(storage_increment_boot_count());

    // Get current value for output
    uint32_t boot_count = storage_get_boot_count();
    ESP_LOGI(TAG, "Boot count: %" PRIu32, boot_count);


    esp_err_t ret;
    data_received = false;

    // Setting power management
    #ifdef CONFIG_PM_ENABLE
        esp_pm_config_t pm_config = {
            .max_freq_mhz = 80,        
            .min_freq_mhz = 40,        
            .light_sleep_enable = false 
        };
        
        ret = esp_pm_configure(&pm_config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Power management configuration failed");
        } else {
            ESP_LOGI(TAG, "Power management configured with max frequency: %d MHz", pm_config.max_freq_mhz);
        }
    #endif

    ret = nvs_flash_init();
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
    }

    // Initialize BLE scanner for RuuviTag
    ESP_LOGI(TAG, "Initializing RuuviTag scanner...");
    ESP_ERROR_CHECK(sensors_init(ruuvi_data_callback));

    // Wait for data or timeout
    const int MAX_WAIT_TIME_MS = 5000;  // 5 seconds maximum
    int waited_ms = 0;
    const int CHECK_INTERVAL_MS = 500;   // Check every 500 ms

    while (!data_received && waited_ms < MAX_WAIT_TIME_MS) {
        vTaskDelay(CHECK_INTERVAL_MS / 10);  //devide by 10 because 1 tick = 10ms
        waited_ms += CHECK_INTERVAL_MS;
    } 

    // Deinitialize BLE before going to sleep
    sensors_deinit();

    if (!data_received) {
        ESP_LOGW(TAG, "No data received within timeout period");
    }

    // Set timer to wake up
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    ESP_LOGI(TAG, "Going to sleep for %d seconds", TIME_TO_SLEEP);

    // Deep sleep
    esp_deep_sleep_start();
}