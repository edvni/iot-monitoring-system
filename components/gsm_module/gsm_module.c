// gsm_module.c 
#include "gsm_module.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "GSM_MODULE";
static TaskHandle_t gsm_task_handle = NULL;

static void gsm_uart_task(void *arg)
{
    char response[BUF_SIZE];
    static bool pin_set = false;
    
    while (1) {
        if (!pin_set) {
            esp_err_t ret = gsm_set_pin("1234");  // Using a PIN code for SIM card
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "PIN set successfully");
                pin_set = true;
            } else {
                ESP_LOGE(TAG, "Failed to set PIN");
            }
        } else {
            // Cheking registration status
            esp_err_t ret = gsm_send_at_cmd("AT+CREG?\r\n", response, sizeof(response), 1000);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "Network registration status: %s", response);
            }
        }
        
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

esp_err_t gsm_module_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GSM_PWRKEY_PIN) | 
                        (1ULL << GSM_RESET_PIN) | 
                        (1ULL << GSM_POWER_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(GSM_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(GSM_UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(GSM_UART_PORT_NUM, GSM_UART_TX_PIN, GSM_UART_RX_PIN, 
                                UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // Step 1: Enable power
    gpio_set_level(GSM_POWER_PIN, 1);
    vTaskDelay(2000 / portTICK_PERIOD_MS);  // Increased delay for power stabilization

    // Step 2: PWRKEY sequence
    gpio_set_level(GSM_PWRKEY_PIN, 0);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    gpio_set_level(GSM_PWRKEY_PIN, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);  // Increased delay as per examples
    gpio_set_level(GSM_PWRKEY_PIN, 0);
    vTaskDelay(2000 / portTICK_PERIOD_MS);  // Wait for module to initialize
    

    xTaskCreate(gsm_uart_task, "gsm_uart_task", 8192, NULL, 10, &gsm_task_handle);

    return ESP_OK;
}

esp_err_t gsm_module_deinit(void)
{
    if (gsm_task_handle) {
        vTaskDelete(gsm_task_handle);
    }
    return uart_driver_delete(GSM_UART_PORT_NUM);
}

esp_err_t gsm_send_at_cmd(const char *cmd, char *response, size_t response_size, uint32_t timeout_ms)
{
    if (!cmd) {
        return ESP_ERR_INVALID_ARG;
    }

    // Flush both input and output
    uart_flush(GSM_UART_PORT_NUM);
    vTaskDelay(100 / portTICK_PERIOD_MS); // Some delay before sending command

    // Send command
    uart_write_bytes(GSM_UART_PORT_NUM, cmd, strlen(cmd));
    ESP_LOGI(TAG, "Sent: %s", cmd);

    if (!response || response_size == 0) {
        return ESP_OK;
    }

    uint8_t *data = (uint8_t *)malloc(BUF_SIZE);
    if (!data) {
        return ESP_ERR_NO_MEM;
    }

    memset(data, 0, BUF_SIZE);
    
    // Retry 3 times if no response
    int total_read = 0;
    int retries = 3;
    
    while (retries--) {
        int len = uart_read_bytes(GSM_UART_PORT_NUM, data + total_read, 
                                BUF_SIZE - total_read - 1, timeout_ms / portTICK_PERIOD_MS);
        if (len > 0) {
            total_read += len;
            data[total_read] = '\0';
            
            // Check for OK or ERROR in response
            if (strstr((char *)data, "OK") || strstr((char *)data, "ERROR")) {
                break;
            }
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    if (total_read > 0) {
        ESP_LOGI(TAG, "Received: %s", (char *)data);
        if (strstr((char *)data, "OK")) {
            strncpy(response, (char *)data, response_size - 1);
            response[response_size - 1] = '\0';
            free(data);
            return ESP_OK;
        }
    }

    free(data);
    return ESP_FAIL;
}

esp_err_t gsm_set_pin(const char* pin)
{
    if (!pin) {
        ESP_LOGE(TAG, "PIN code is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    char cmd[32];
    char response[BUF_SIZE];
    
    // Cheking current PIN status
    esp_err_t ret = gsm_send_at_cmd("AT+CPIN?\r\n", response, sizeof(response), 1000);
    if (ret == ESP_OK && strstr(response, "READY")) {
        ESP_LOGI(TAG, "SIM already unlocked");
        return ESP_OK;
    }

    // If not READY, try to set PIN
    snprintf(cmd, sizeof(cmd), "AT+CPIN=\"%s\"\r\n", pin);
    ret = gsm_send_at_cmd(cmd, response, sizeof(response), 5000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set PIN");
        return ESP_FAIL;
    }
    
    // Some delay before checking status
    vTaskDelay(500 / portTICK_PERIOD_MS);
    
    // Check status after setting PIN
    ret = gsm_send_at_cmd("AT+CPIN?\r\n", response, sizeof(response), 1000);
    if (ret == ESP_OK && strstr(response, "READY")) {
        ESP_LOGI(TAG, "PIN accepted, SIM ready");
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "PIN verification failed");
    return ESP_FAIL;
}
