#include "driver/uart.h"
#include "hal/uart_types.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "esp_netif_defaults.h"
#include "esp_modem_dce_config.h"
#include "gsm_module.h"
#include "esp_modem_api.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "gsm_module";
static esp_modem_dce_t *dce = NULL;



esp_err_t gsm_module_init(void) {
    esp_log_level_set(TAG, ESP_LOG_INFO);
    

    esp_netif_config_t netif_config = ESP_NETIF_DEFAULT_PPP();


    esp_netif_t *netif = esp_netif_new(&netif_config);
    if (netif == NULL) {
        ESP_LOGE(TAG, "Не удалось создать netif");
        return ESP_FAIL;
    }



    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();

    dte_config.uart_config.port_num = UART_NUM_2; // Порт UART2
    dte_config.uart_config.data_bits = UART_DATA_8_BITS; // 8 бит данных   
    dte_config.uart_config.stop_bits = UART_STOP_BITS_1 ; // 1 стоп-бит    
    dte_config.uart_config.baud_rate = 115200;       
    dte_config.uart_config.tx_io_num = GSM_UART_TX_PIN;  // TX пин
    dte_config.uart_config.rx_io_num = GSM_UART_RX_PIN;  // RX пин
    dte_config.uart_config.rx_buffer_size = BUF_SIZE;    // Размер буфера приема
    dte_config.uart_config.tx_buffer_size = BUF_SIZE;    // Размер буфера передачи
    dte_config.uart_config.event_queue_size = 50;        // Размер очереди событий UART
    dte_config.uart_config.flow_control = UART_HW_FLOWCTRL_DISABLE; // Отключение аппаратного контроля потока

    ESP_LOGI(TAG, "UART Config:");
    ESP_LOGI(TAG, "  Port: %d", dte_config.uart_config.port_num);
    ESP_LOGI(TAG, "  Baud Rate: %d", dte_config.uart_config.baud_rate);
    ESP_LOGI(TAG, "  Data Bits: %d", dte_config.uart_config.data_bits);
    ESP_LOGI(TAG, "  Stop Bits: %d", dte_config.uart_config.stop_bits);
    ESP_LOGI(TAG, "  TX Pin: %d", dte_config.uart_config.tx_io_num);
    ESP_LOGI(TAG, "  RX Pin: %d", dte_config.uart_config.rx_io_num);
    ESP_LOGI(TAG, "  RX Buffer Size: %d", dte_config.uart_config.rx_buffer_size);
    ESP_LOGI(TAG, "  TX Buffer Size: %d", dte_config.uart_config.tx_buffer_size);
    ESP_LOGI(TAG, "  Queue Size: %d", dte_config.uart_config.event_queue_size);


    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(GSM_APN);




    dce = esp_modem_new_dev(ESP_MODEM_DCE_GENERIC,
                            &dte_config,    // конфигурация DTE
                            &dce_config,    // конфигурация DCE   
                            netif            // сетевой интерфейс (NULL для базовой инициализации)
    );
    
    return ESP_OK;
}