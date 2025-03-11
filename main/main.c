#include <stdio.h>
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_pm.h" 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/rtc_cntl_reg.h"

#include "power_management.h"
#include "system_state.h"
#include "sensors.h"
#include "storage.h"
#include "gsm_modem.h"
#include "discord_api.h"
#include "discord_config.h"


static const char *TAG = "main";

#define TIME_TO_SLEEP    1200        // Time in seconds to go to sleep        
#define SECONDS_PER_DAY  86400      // 24 hours in seconds
#define uS_TO_S_FACTOR   1000000ULL // Conversion factor for micro seconds to seconds
#define CONFIG_NIMBLE_CPP_LOG_LEVEL 0
#define SEND_DATA_CYCLE 5  // For testing 3

static volatile bool data_received = false;  // Flag for data received
battery_status_t battery;
char message[128] = "ESP32 started successfully- The measurements for the next part of the day have started";
int i = 0;

// Discord configuration
static const discord_config_t discord_cfg = {
    .bot_token = DISCORD_BOT_TOKEN,
    .channel_id = DISCORD_CHANNEL_ID
};

// RuuviTag data callback
static void ruuvi_data_callback(ruuvi_measurement_t *measurement) {
    static bool data_saved = false;  // Status flag for tracking saved data

    if (data_saved) {
        return;
    }
    
    // ESP_LOGI(TAG, "RuuviTag data received");
    // ESP_LOGI(TAG, "  MAC: %s", measurement->mac_address);
    // ESP_LOGI(TAG, "  Temperature: %.2f °C", measurement->temperature);
    // ESP_LOGI(TAG, "  Humidity: %.2f %%", measurement->humidity);
    // ESP_LOGI(TAG, "  Timestamp: %llu", measurement->timestamp);

    // Mearurements saiving
    esp_err_t ret = storage_save_measurement(measurement);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save measurement");
    } else {
        data_saved = true;     // Mark that we saved
        data_received = true;  // Set the flag for the main cycle
    }
}

// Unsuccessful initialization
static void unsuccessful_init() {
    //ESP_LOGE(TAG, "Failed to initialize GSM or Discord API");
    storage_append_log("Unsuccessful initialization detected");
    storage_set_error_flag();
    modem_power_off();
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
}

// Main function
void app_main(void)
{               
    // Variables
    esp_err_t ret;
    char log_buf[64];
    bool network_initialized = false;
    bool data_from_storage_sent = false;
    bool first_boot = false;
    //WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    
    // Power management initialization
    ESP_ERROR_CHECK(power_management_init());
    
    
    // Storage initialization
    ESP_ERROR_CHECK(storage_init());

    // Get the current system state
    system_state_t current_state = storage_get_system_state();
    
    // For debugging, add a log about the current state
    snprintf(log_buf, sizeof(log_buf), "Current system state: %d", current_state);
    storage_append_log(log_buf);

    // Counter checking
    uint32_t boot_count = storage_get_boot_count();
    sniprintf(log_buf, sizeof(log_buf), "Boot count before work cycle: %lu", boot_count);
    storage_append_log(log_buf);
    
    // Processing system states
    switch (current_state) {
        case STATE_FIRST_BLOCK_RECOVERY:
            storage_append_log("Entering first block recovery mode");
            goto first_block_init;
            
        case STATE_SECOND_BLOCK_RECOVERY:
            storage_append_log("Entering second block recovery mode");
            goto second_block_init;
            
        case STATE_THIRD_BLOCK_RECOVERY:
            storage_append_log("Entering third block recovery mode");
            goto third_block_init;
            
        case STATE_NORMAL:
        default:
            // Continue normal execution //
            break;
    }

    
    // --- BLOCK 1: Special operations for the first boot ---
    if (boot_count == 0) {
        first_boot = true; 

        storage_append_log("First boot operations");
        
        // Setting the state for the first block
        storage_set_system_state(STATE_FIRST_BLOCK_RECOVERY);
        vTaskDelay(pdMS_TO_TICKS(500));

first_block_init:

        // Modem initialization for the first message
        ret = gsm_modem_init();
        if (ret != ESP_OK) {
            storage_append_log("GSM modem init failed in first boot");
            unsuccessful_init();
        } else {
            network_initialized = true;
        }

        // Setting the normal state
        storage_set_system_state(STATE_NORMAL);
        vTaskDelay(pdMS_TO_TICKS(500));

        // Discord API initialization for the first message
        ret = discord_init(&discord_cfg);
        if (ret != ESP_OK) {
            storage_append_log("Discord init failed in first boot");
            unsuccessful_init();
        }
        // Sending the first message
        ret = discord_send_message(message);
        if (ret != ESP_OK) {
            storage_append_log("Failed to send first boot message");
        } 

        storage_append_log("Done");
        
    }
    
// --- BLOCK 2: Data collection (for all cycles) ---
// data_collection:
    // Mark that there was an error in the previous cycle
    bool error_in_prev_cycle = storage_get_error_flag();    
    if(!error_in_prev_cycle) {
        storage_append_log("Starting data collection");
        data_received = false;
        // Sensors initialization
        ESP_ERROR_CHECK(sensors_init(ruuvi_data_callback));
        
        // Waiting for data
        const int MAX_WAIT_TIME_MS = 10000;
        int waited_ms = 0;
        const int CHECK_INTERVAL_MS = 500;
        
        while (!data_received && waited_ms < MAX_WAIT_TIME_MS) {
            vTaskDelay(pdMS_TO_TICKS(CHECK_INTERVAL_MS));
            waited_ms += CHECK_INTERVAL_MS;
        }
        
        if (!data_received) {
            storage_append_log("Failed to receive sensor data");
        } 

        // Increment the counter 
        ESP_ERROR_CHECK(storage_increment_boot_count());
          
        storage_append_log("Done");

    } else {

        storage_append_log("Error in previous cycle was detected, skipping data collection");
    }

    boot_count = storage_get_boot_count();

// --- BLOCK 3: Send accumulated data if target value is reached ---
    if (boot_count >= SEND_DATA_CYCLE) {
        storage_append_log("Sending accumulated data");

        // Setting the state for the second block
        storage_set_system_state(STATE_SECOND_BLOCK_RECOVERY);
        vTaskDelay(pdMS_TO_TICKS(500));

second_block_init:

        // Modem initialization for data sending if it was not initialized
        if (!network_initialized) {
            ret = gsm_modem_init();
            if (ret != ESP_OK) {
                storage_append_log("GSM modem init failed for data sending");
                unsuccessful_init();
            } else {
                network_initialized = true;
            }

            // Setting the normal state
            storage_set_system_state(STATE_NORMAL);
            vTaskDelay(pdMS_TO_TICKS(500));

            // Discord API initialization for data sending
            ret = discord_init(&discord_cfg);
            if (ret != ESP_OK) {
                storage_append_log("Discord init failed for data sending");
                unsuccessful_init();
            }
        }
        // Getting measurements from storage and sending them
        if (network_initialized) {
            
            char *measurements = storage_get_measurements();
            if (measurements != NULL) {
                ret = send_measurements_with_retries(measurements, 3);
                free(measurements);
                
                if (ret == ESP_OK) {
                    ESP_ERROR_CHECK(storage_clear_measurements());
                    ESP_ERROR_CHECK(storage_reset_counter());
                    data_from_storage_sent = true;
                } else {
                    storage_append_log("Failed to send measurements");
                }
            }
        }
            
        storage_append_log("Done");
        
    }
    
// --- BLOCK 4: Terminate the loop and send logs ---
// end_cycle:
    // Deinitialization of sensors
    sensors_deinit();

    snprintf(log_buf, sizeof(log_buf), "Final boot count: %lu", storage_get_boot_count());
    storage_append_log(log_buf);
    
    
    // Sending logs if data was sent
    if (data_from_storage_sent && !network_initialized) {
        storage_append_log("Sending logs");

        // Setting the state for the third block
        storage_set_system_state(STATE_THIRD_BLOCK_RECOVERY);
        vTaskDelay(pdMS_TO_TICKS(500));

third_block_init:

        // Modem initialization for logs sending
        ret = gsm_modem_init();
        if (ret != ESP_OK) {
            storage_append_log("GSM modem init failed for logs");
            goto sleep_prepare;
        } else {
            network_initialized = true;
        }

        // Setting the normal state
        storage_set_system_state(STATE_NORMAL);
        vTaskDelay(pdMS_TO_TICKS(500));
        
        // Discord API initialization for logs sending
        ret = discord_init(&discord_cfg);
        if (ret != ESP_OK) {
            storage_append_log("Discord API init failed for logs");
            gsm_modem_deinit();
            goto sleep_prepare;
        }
    }
    
    // Sending logs if data was sent to Discord
    if (data_from_storage_sent) {
        send_logs_with_retries(3);
    }

    // Final deinitialization of GSM modem
    if (first_boot && network_initialized) {
        gsm_modem_deinit();
    } else if (network_initialized && data_from_storage_sent) {
        gsm_modem_deinit();
    }
    
    storage_set_system_state(STATE_NORMAL);
    
    // Preparing for sleep
sleep_prepare:
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    ESP_LOGI(TAG, "Going to sleep for %d seconds", TIME_TO_SLEEP);
    esp_deep_sleep_start();
}



//     // /*------------For debuging---------------*/
//     // // Print measurements in debug mode
//     // char *measurements = storage_get_measurements();
//     // if (measurements != NULL) {
//     //     //ESP_LOGI(TAG, "Stored data: %s", measurements);
//     //     free(measurements);
//     // }
//     // /*---------------------------------------*/




