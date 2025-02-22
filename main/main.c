#include <stdio.h>
#include <inttypes.h>
#include <time.h>
#include <sys/time.h>  
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_sleep.h"
#include "esp_mac.h"
#include "esp_pm.h"
#include "esp_event.h"

#include "main_config.h"

#include "gsm_module.h"
#include "sensors.h"
#include "storage.h"
#include "time_manager.h"
#include "power_management.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include <string.h>
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "esp_modem_api.h"
#include "sdkconfig.h"



static const char *TAG = "MAIN";

static EventGroupHandle_t event_group = NULL;
static const int CONNECT_BIT = BIT0;
static const int DISCONNECT_BIT = BIT1;

// Обработчики событий PPP и IP (оставляем как есть)
static void on_ppp_changed(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "PPP state changed event %" PRIu32, event_id);
    if (event_id == NETIF_PPP_ERRORUSER) {
        esp_netif_t **p_netif = event_data;
        ESP_LOGI(TAG, "User interrupted event from netif:%p", *p_netif);
    }
}

static void on_ip_event(void *arg, esp_event_base_t event_base,
                       int32_t event_id, void *event_data)
{
    if (event_id == IP_EVENT_PPP_GOT_IP) {
        esp_netif_dns_info_t dns_info;
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        esp_netif_t *netif = event->esp_netif;

        ESP_LOGI(TAG, "Modem Connect to PPP Server");
        ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
        ESP_LOGI(TAG, "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
        ESP_LOGI(TAG, "Gateway     : " IPSTR, IP2STR(&event->ip_info.gw));
        esp_netif_get_dns_info(netif, 0, &dns_info);
        ESP_LOGI(TAG, "Name Server1: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        esp_netif_get_dns_info(netif, 1, &dns_info);
        ESP_LOGI(TAG, "Name Server2: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
        xEventGroupSetBits(event_group, CONNECT_BIT);
    } else if (event_id == IP_EVENT_PPP_LOST_IP) {
        ESP_LOGI(TAG, "Modem Disconnect from PPP Server");
        xEventGroupSetBits(event_group, DISCONNECT_BIT);
    }
}

void app_main(void)
{
    // Инициализация системных компонентов
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &on_ip_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, NULL));

    // Конфигурация PPP netif
    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(MODEM_PPP_APN);
    esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();
    esp_netif_t *esp_netif = esp_netif_new(&netif_ppp_config);
    assert(esp_netif);

    event_group = xEventGroupCreate();

    // Конфигурация DTE
    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    // Настройка UART
    dte_config.uart_config.tx_io_num = MODEM_UART_TX_PIN;
    dte_config.uart_config.rx_io_num = MODEM_UART_RX_PIN;
    dte_config.uart_config.flow_control = MODEM_FLOW_CONTROL;
    dte_config.uart_config.rx_buffer_size = MODEM_UART_RX_BUFFER_SIZE;
    dte_config.uart_config.tx_buffer_size = MODEM_UART_TX_BUFFER_SIZE;
    dte_config.uart_config.event_queue_size = MODEM_UART_EVENT_QUEUE_SIZE;
    dte_config.task_stack_size = MODEM_UART_EVENT_TASK_STACK_SIZE;
    dte_config.task_priority = MODEM_UART_EVENT_TASK_PRIORITY;
    dte_config.dte_buffer_size = MODEM_UART_RX_BUFFER_SIZE / 2;

    // Инициализация модема A7670
    ESP_LOGI(TAG, "Initializing esp_modem for the A7670 module...");
    esp_modem_dce_t *dce = esp_modem_new(&dte_config, &dce_config, esp_netif);
    assert(dce);

    // Проверка PIN-кода (если требуется)
    #if CONFIG_EXAMPLE_NEED_SIM_PIN == 1
    bool pin_ok = false;
    if (esp_modem_read_pin(dce, &pin_ok) == ESP_OK && pin_ok == false) {
        if (esp_modem_set_pin(dce, CONFIG_EXAMPLE_SIM_PIN) == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else {
            ESP_LOGE(TAG, "Pin setting failed");
            return;
        }
    }
    #endif

    // Получение уровня сигнала
    int rssi, ber;
    esp_err_t err = esp_modem_get_signal_quality(dce, &rssi, &ber);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get signal quality");
        return;
    }
    ESP_LOGI(TAG, "Signal quality: rssi=%d, ber=%d", rssi, ber);

    // Установление PPP соединения
    err = esp_modem_set_mode(dce, ESP_MODEM_MODE_DATA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set PPP mode");
        return;
    }

    // Ожидание получения IP-адреса
    ESP_LOGI(TAG, "Waiting for IP address");
    xEventGroupWaitBits(event_group, CONNECT_BIT | DISCONNECT_BIT, pdFALSE, pdFALSE,
                        pdMS_TO_TICKS(60000));

    // Проверка статуса подключения
    if ((xEventGroupGetBits(event_group) & CONNECT_BIT) != CONNECT_BIT) {
        ESP_LOGW(TAG, "PPP connection failed");
        err = esp_modem_set_mode(dce, ESP_MODEM_MODE_COMMAND);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to restore command mode");
        }
        return;
    }

    // Теперь соединение установлено и можно использовать PPP
    ESP_LOGI(TAG, "PPP connection established");

    // Здесь можно добавить основной код приложения

    // Очистка ресурсов
    esp_modem_destroy(dce);
    esp_netif_destroy(esp_netif);
}



// #define TIME_TO_SLEEP    600        // Time in seconds to go to sleep        
// #define SECONDS_PER_DAY  86400      // 24 hours in seconds
// #define uS_TO_S_FACTOR   1000000ULL // Conversion factor for micro seconds to seconds
// #define CONFIG_NIMBLE_CPP_LOG_LEVEL 0 // Disable NimBLE logs



// static volatile bool data_received = false;  // Flag for data received

// static void ruuvi_data_callback(ruuvi_measurement_t *measurement) {
//    ESP_LOGI(TAG, "RuuviTag data:");
//    ESP_LOGI(TAG, "  MAC: %s", measurement->mac_address);
//    ESP_LOGI(TAG, "  Temperature: %.2f °C", measurement->temperature);
//    ESP_LOGI(TAG, "  Humidity: %.2f %%", measurement->humidity);
//    ESP_LOGI(TAG, "  Timestamp: %s", measurement->timestamp);

//    // Save measurement to SPIFFS
//    esp_err_t ret = storage_save_measurement(measurement);
//    if (ret != ESP_OK) {
//        ESP_LOGE(TAG, "Failed to save measurement");
//    }

//    data_received = true;
// }














    
    // // Power management initialization
    // ESP_ERROR_CHECK(power_management_init());

    // // Storage initialization  
    // ESP_ERROR_CHECK(storage_init());

    // // Check if this is first boot
    // if (storage_is_first_boot()) {
    //     ESP_LOGI(TAG, "First boot detected, initializing systems");
        
        
    //     // Initialize GSM module and try to connect
    //     ESP_ERROR_CHECK(gsm_module_init());
        
        
    //     // Mark first boot as completed
    //     ESP_ERROR_CHECK(storage_set_first_boot_completed());
    //     ESP_LOGI(TAG, "First boot setup completed");
    // }

    // // Increment and save the counter
    // ESP_ERROR_CHECK(storage_increment_boot_count());

    // // Check and reset counter if 24 hours passed
    // ESP_ERROR_CHECK(storage_check_and_reset_counter(TIME_TO_SLEEP));

    // Get current value for output
//     uint32_t boot_count = storage_get_boot_count();
//     ESP_LOGI(TAG, "Boot count: %" PRIu32, boot_count);
   
//    data_received = false;

   // Initialize BLE scanner for RuuviTag
//    ESP_LOGI(TAG, "Initializing RuuviTag scanner...");
//    ESP_ERROR_CHECK(sensors_init(ruuvi_data_callback));

   // Wait for data or timeout
//    const int MAX_WAIT_TIME_MS = 10000;  // 10 seconds maximum
//    int waited_ms = 0;
//    const int CHECK_INTERVAL_MS = 500;   // Check every 500 ms

//    while (!data_received && waited_ms < MAX_WAIT_TIME_MS) {
//        vTaskDelay(CHECK_INTERVAL_MS / portTICK_PERIOD_MS);
//        waited_ms += CHECK_INTERVAL_MS;
//    } 

   // Print measurements in debug mode
//    char *measurements = storage_get_measurements();
//    if (measurements != NULL) {
//        ESP_LOGI(TAG, "Stored data: %s", measurements);
//        free(measurements);
//    }

   // Deinitialize BLE before going to sleep
//    sensors_deinit();

   // Deinitialize GSM module before sleep
   // gsm_module_deinit();

//    if (!data_received) {
//        ESP_LOGW(TAG, "No data received within timeout period");
//    }

//    // Set timer to wake up
//    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
//    ESP_LOGI(TAG, "Going to sleep for %d seconds", TIME_TO_SLEEP);

//    // Deep sleep
//    esp_deep_sleep_start();
