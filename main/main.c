#include <stdio.h>
#include <inttypes.h>
#include <time.h>
#include <sys/time.h>  
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_sleep.h"
#include "esp_mac.h"
#include "esp_pm.h"

#include "gsm_module.h"
#include "sensors.h"
#include "storage.h"
#include "time_manager.h"
#include "power_management.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TIME_TO_SLEEP    600        // Time in seconds to go to sleep        
#define SECONDS_PER_DAY  86400      // 24 hours in seconds
#define uS_TO_S_FACTOR   1000000ULL // Conversion factor for micro seconds to seconds
#define CONFIG_NIMBLE_CPP_LOG_LEVEL 0 // Disable NimBLE logs

static const char *TAG = "MAIN";
static volatile bool data_received = false;  // Flag for data received

static void ruuvi_data_callback(ruuvi_measurement_t *measurement) {
   ESP_LOGI(TAG, "RuuviTag data:");
   ESP_LOGI(TAG, "  MAC: %s", measurement->mac_address);
   ESP_LOGI(TAG, "  Temperature: %.2f °C", measurement->temperature);
   ESP_LOGI(TAG, "  Humidity: %.2f %%", measurement->humidity);
   ESP_LOGI(TAG, "  Timestamp: %s", measurement->timestamp);

   // Save measurement to SPIFFS
   esp_err_t ret = storage_save_measurement(measurement);
   if (ret != ESP_OK) {
       ESP_LOGE(TAG, "Failed to save measurement");
   }

   data_received = true;
}

void app_main(void)
{   
    esp_log_level_set(TAG, ESP_LOG_INFO);
    
    // Power management initialization
    ESP_ERROR_CHECK(power_management_init());

    // Storage initialization  
    ESP_ERROR_CHECK(storage_init());

    // Check if this is first boot
    if (storage_is_first_boot()) {
        ESP_LOGI(TAG, "First boot detected, initializing systems");
        
        
        // Initialize GSM module and try to connect
        ESP_ERROR_CHECK(gsm_module_init());
        
        
        // Mark first boot as completed
        ESP_ERROR_CHECK(storage_set_first_boot_completed());
        ESP_LOGI(TAG, "First boot setup completed");
    }

    // Increment and save the counter
    ESP_ERROR_CHECK(storage_increment_boot_count());

    // Check and reset counter if 24 hours passed
    ESP_ERROR_CHECK(storage_check_and_reset_counter(TIME_TO_SLEEP));

    // Get current value for output
    uint32_t boot_count = storage_get_boot_count();
    ESP_LOGI(TAG, "Boot count: %" PRIu32, boot_count);
   
   data_received = false;

   // Initialize BLE scanner for RuuviTag
   ESP_LOGI(TAG, "Initializing RuuviTag scanner...");
   ESP_ERROR_CHECK(sensors_init(ruuvi_data_callback));

   // Wait for data or timeout
   const int MAX_WAIT_TIME_MS = 10000;  // 10 seconds maximum
   int waited_ms = 0;
   const int CHECK_INTERVAL_MS = 500;   // Check every 500 ms

   while (!data_received && waited_ms < MAX_WAIT_TIME_MS) {
       vTaskDelay(CHECK_INTERVAL_MS / portTICK_PERIOD_MS);
       waited_ms += CHECK_INTERVAL_MS;
   } 

   // Print measurements in debug mode
   char *measurements = storage_get_measurements();
   if (measurements != NULL) {
       ESP_LOGI(TAG, "Stored data: %s", measurements);
       free(measurements);
   }

   // Deinitialize BLE before going to sleep
   sensors_deinit();

   // Deinitialize GSM module before sleep
   // gsm_module_deinit();

   if (!data_received) {
       ESP_LOGW(TAG, "No data received within timeout period");
   }

//    // Set timer to wake up
//    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
//    ESP_LOGI(TAG, "Going to sleep for %d seconds", TIME_TO_SLEEP);

//    // Deep sleep
//    esp_deep_sleep_start();
}