#include <stdio.h>

// Global logging definitions. If true, logging will be enabled.
#define DISCORD_LOGGING false
#define SYSTEM_LOGGING true

#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_pm.h" 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "soc/rtc_cntl_reg.h"
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include "cJSON.h"

#include "power_management.h"
#include "state.h"
#include "sensors.h"
#include "storage.h"
#include "gsm_modem.h"
#include "discord_api.h"
#include "discord_config.h"
#include "esp_timer.h"
#include "time_manager.h"
#include "battery_monitor.h"
#include "reporter.h"
#include "firebase_api.h"


static const char *TAG = "main";
#define CONFIG_NIMBLE_CPP_LOG_LEVEL 0
#define SECONDS_IN_MICROS 1000000ULL
#define SEND_DATA_CYCLE 144  // For testing 3
#define TRIGGER_INTERVAL    (3 * SECONDS_IN_MICROS) // defined in seconds (600 seconds)

static volatile bool data_received = false;  // Flag for data received

// RuuviTag data callback
static void ruuvi_data_callback(ruuvi_measurement_t *measurement) {
    esp_err_t ret = storage_save_measurement(measurement);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save measurement from %s", measurement->mac_address);
    } else {
        ESP_LOGI(TAG, "Successfully saved data from %s", measurement->mac_address);
        data_received = true;  // Set the flag for the main cycle
    }
}


// Main function
void app_main(void)
{   
    #if !SYSTEM_LOGGING
        ESP_LOGI(TAG, "Logging is disabled");
        ESP_LOGI(TAG, "Program started");
        esp_log_level_set("*", ESP_LOG_NONE);
    #endif

    // Save start time and trigger time
    int64_t start_time = esp_timer_get_time();
    vTaskDelay(pdMS_TO_TICKS(100)); // Give time for NVS to save

    // Variables
    esp_err_t ret;
    char log_buf[64];
    bool network_initialized = false;
    bool data_from_storage_sent = false;
    bool first_boot = false;
    //WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    
    // Power management initialization
    ESP_ERROR_CHECK(power_management_init());
    
    // Initialize battery monitor
    ESP_ERROR_CHECK(battery_monitor_init());
    
    // Time manager initialization
    time_manager_set_finland_timezone();
    
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
            
        #if DISCORD_LOGGING
            case STATE_THIRD_BLOCK_RECOVERY:
                storage_append_log("Entering third block recovery mode");
                goto third_block_init;
        #endif
            
        case STATE_NORMAL:
        default:
            // Continue normal execution //
            break;
    }

    // --- BLOCK 1: Special operations for the first boot ---
    if (storage_is_first_boot()) {
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
            unsuccessful_init(TAG);
        } else {
            network_initialized = true;

            // Add time synchronization
            time_t network_time = gsm_get_network_time();
            if (network_time > 0) {
                time_manager_set_from_timestamp(network_time); // Synchronize time
            } else {
                storage_append_log("Failed to synchronize time with NTP");
            }
        }

        // Setting the normal state
        storage_set_system_state(STATE_NORMAL);
        vTaskDelay(pdMS_TO_TICKS(500));

        // Discord API initialization for the first message
        ret = sending_report_to_discord();
        if (ret != ESP_OK) {
            storage_append_log("Failed to send first boot message");
        }
        
        // Mark first boot as completed
        storage_mark_first_boot_completed();
        storage_append_log("First boot completed");
    }
    
// --- BLOCK 2: Data collection (for all cycles) ---
// data_collection:
    // Mark that there was an error in the previous cycle
    bool error_in_prev_cycle = storage_get_error_flag();    
    if(!error_in_prev_cycle) {
        storage_append_log("Starting data collection");
        
        // Declaration of variables for the retry mechanism
        const int MAX_SCAN_ATTEMPTS = 3;
        int scan_attempt = 0;
        const int TOTAL_SENSORS = sensors_get_total_count();
        bool all_sensors_received = false;
        
        // Cycle of repeated attempts to scan
        while (scan_attempt < MAX_SCAN_ATTEMPTS && !all_sensors_received) {
            if (scan_attempt > 0) {
                ESP_LOGI(TAG, "Starting scan attempt %d of %d", scan_attempt + 1, MAX_SCAN_ATTEMPTS);
                storage_append_log("Restarting sensor scan");
                
                // Stop the previous scan and reinitialize the sensors
                sensors_deinit();
                
                // Reset the sensor status
                sensors_reset_status();
                
                vTaskDelay(pdMS_TO_TICKS(500)); // Small pause between attempts
            }
            
            // Sensors initialization
            ESP_ERROR_CHECK(sensors_init(ruuvi_data_callback));
            
            // Waiting for data
            const int MAX_WAIT_TIME_MS = 10000;
            int waited_ms = 0;
            const int CHECK_INTERVAL_MS = 500;
            
            // Wait until all sensors have sent data or the timeout expires
            while (sensors_get_received_count() < TOTAL_SENSORS && waited_ms < MAX_WAIT_TIME_MS) {
                vTaskDelay(pdMS_TO_TICKS(CHECK_INTERVAL_MS));
                waited_ms += CHECK_INTERVAL_MS;
                
                // Log progress every 2.5 seconds
                if (waited_ms % 2500 == 0) {
                    ESP_LOGI(TAG, "Waiting for sensors: %d/%d received, waited %d ms", 
                             sensors_get_received_count(), TOTAL_SENSORS, waited_ms);
                }
            }
            
            // Check if all sensors are detected
            all_sensors_received = (sensors_get_received_count() == TOTAL_SENSORS);
            
            if (!all_sensors_received) {
                char log_message[64];
                snprintf(log_message, sizeof(log_message), "Incomplete scan: %d/%d sensors, attempt %d", 
                        sensors_get_received_count(), TOTAL_SENSORS, scan_attempt + 1);
                storage_append_log(log_message);
                ESP_LOGW(TAG, "%s", log_message);
            }
            
            scan_attempt++;
        }
        
        // Write to log information about received sensors
        if (!sensors_any_data_received()) {
            storage_append_log("Failed to receive any sensor data");
        } else {
            char log_message[64];
            snprintf(log_message, sizeof(log_message), "Received data from %d/%d sensors after %d attempts", 
                    sensors_get_received_count(), TOTAL_SENSORS, scan_attempt);
            storage_append_log(log_message);
            ESP_LOGI(TAG, "%s", log_message);
            
            if (sensors_get_received_count() == TOTAL_SENSORS) {
                ESP_LOGI(TAG, "Successfully received data from all sensors");
            } else {
                ESP_LOGW(TAG, "Some sensors did not respond: %d/%d received", 
                         sensors_get_received_count(), TOTAL_SENSORS);
            }
            
            data_received = true;
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
        // Modem initialization for data sending
        ret = gsm_modem_init();
        if (ret != ESP_OK) {
            storage_append_log("GSM modem init failed for data sending");
            unsuccessful_init(TAG);
        } else {
            network_initialized = true;
        }

        // Add time synchronization
        time_t network_time = gsm_get_network_time();
        if (network_time > 0) {
            time_manager_set_from_timestamp(network_time); // Synchronize time
        } else {
            storage_append_log("Failed to synchronize time with NTP");
        }

        // Setting the normal state
        storage_set_system_state(STATE_NORMAL);
        vTaskDelay(pdMS_TO_TICKS(500));

        // Firebase API initialization 
        ret = firebase_init();
        if (ret != ESP_OK) {
            storage_append_log("Firebase init failed for data sending");
            unsuccessful_init(TAG);
        }
    
        // Getting measurements from storage and sending them
        if (network_initialized) {
            // Send data from all sensors to the server
            ret = send_all_sensor_measurements_to_firebase();
            
            if (ret == ESP_OK) {
                // All files sent successfully
                storage_append_log("All sensor files sent successfully");
                ESP_ERROR_CHECK(storage_reset_counter());
                data_from_storage_sent = true;
            } else if (ret == ESP_ERR_INVALID_STATE) {
                // Some files were sent
                storage_append_log("Some sensor files were not sent");
                data_from_storage_sent = true;
            } else if (ret == ESP_ERR_NOT_FOUND) {
                // Files not found
                storage_append_log("No sensor files found");
            } else {
                // General error
                storage_append_log("Failed to send any files");
            }
        }

        // Discord message about battery status
        ret = sending_report_to_discord();
        if (ret != ESP_OK) {
            storage_append_log("Failed to send message about battery status");
        }
            
        storage_append_log("Done");
        
    }
    
// --- BLOCK 4: Terminate the loop and send logs ---
// end_cycle:
    // Deinitialization of sensors
    sensors_deinit();

    // Safe string formatting with boot count
    int written = snprintf(log_buf, sizeof(log_buf), "Final boot count: %lu", storage_get_boot_count());
    if (written < 0 || written >= (int)sizeof(log_buf)) {
        ESP_LOGE(TAG, "Error formatting boot count log");
        storage_append_log("Error logging final boot count");
    } else {
        storage_append_log(log_buf);
    }
    
    
    // Sending logs if data was sent
    #if DISCORD_LOGGING
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

                // Add time synchronization
                time_t network_time = gsm_get_network_time();
                if (network_time > 0) {
                    time_manager_set_from_timestamp(network_time); // Synchronize time
                } else {
                    storage_append_log("Failed to synchronize time with NTP");
                }
            }

            // Setting the normal state
            storage_set_system_state(STATE_NORMAL);
            vTaskDelay(pdMS_TO_TICKS(500));
            
            // Discord API initialization for logs sending
            ret = discord_init();
            if (ret != ESP_OK) {
                storage_append_log("Discord API init failed for logs");
                gsm_modem_deinit();
                goto sleep_prepare;
            }
        }
    #endif
    

    // Final deinitialization of GSM modem
    if (first_boot && network_initialized) {
        gsm_modem_deinit();
    } else if (network_initialized && data_from_storage_sent) {
        gsm_modem_deinit();
    }
    
    storage_set_system_state(STATE_NORMAL);
    
#if DISCORD_LOGGING
    // Preparing for sleep
    sleep_prepare:
#endif
    // Synchronizing the file system before sleep to ensure the saving of all data
    ESP_ERROR_CHECK(storage_sync());
    
    // Calculate execution time and remaining sleep time
    int64_t current_time = esp_timer_get_time();
    int64_t execution_time = current_time - start_time;
    int64_t sleep_time = TRIGGER_INTERVAL - execution_time;
    if (sleep_time < 0) {
        sleep_time = 0;
    }
    
    ESP_LOGI(TAG, "Trigger interval: %.2f seconds", (float)TRIGGER_INTERVAL / 1000000.0f);
    ESP_LOGI(TAG, "Execution time: %.2f seconds", (float)execution_time / 1000000.0f);
    ESP_LOGI(TAG, "Going to sleep for %.2f seconds", (float)sleep_time / 1000000.0f);
    
    
    esp_sleep_enable_timer_wakeup(sleep_time);
    esp_deep_sleep_start();
}






