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

/*--------------------------------------------------------------------------------*/
/*---------------------------Service functions------------------------------------*/
/*--------------------------------------------------------------------------------*/

// Clear input and output buffer and small delay
static void gsm_uart_flush_and_delay(void)
{
    uart_flush_input(GSM_UART_PORT_NUM);
    uart_flush(GSM_UART_PORT_NUM);
    vTaskDelay(100 / portTICK_PERIOD_MS);
}

// Template for sending AT commands
esp_err_t gsm_send_at_cmd(const char *cmd, char *response, size_t response_size, uint32_t timeout_ms)
{
    if (!cmd) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Clear input and output buffer and small delay
    gsm_uart_flush_and_delay();

    // Sending command
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

    int total_read = 0;
    int attempts = timeout_ms / 100;// Read every 100 ms

    while (attempts--) {
        int len = uart_read_bytes(GSM_UART_PORT_NUM, data + total_read, BUF_SIZE - total_read - 1, 100 / portTICK_PERIOD_MS);
        if (len > 0) {
            total_read += len;
            data[total_read] = '\0';
            // If found "OK" or "ERROR" response, stop reading
            if (strstr((char *)data, "OK") || strstr((char *)data, "ERROR")) {
                break;
            }
        }
    }
    
    // If response is not empty, copy it to the output buffer and free memory
    if (total_read > 0) {
        ESP_LOGI(TAG, "Received: %s", data);
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

// Start setting for using GSM module
esp_err_t gsm_set_pin(const char* pin)
{
    if (!pin) {
        ESP_LOGE(TAG, "PIN code is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    char response[BUF_SIZE];
    char cmd[32];

    // Clear input and output buffer and small delay
    gsm_uart_flush_and_delay();

    // 1. Turning off echo
    if (gsm_send_at_cmd("ATE0\r\n", response, sizeof(response), 1000) == ESP_OK) {
        ESP_LOGI(TAG, "Echo disabled: %s", response);
    } else {
        ESP_LOGW(TAG, "Failed to disable echo, proceeding anyway...");
    }

    // 2. Checking SIM status
    if (gsm_send_at_cmd("AT+CPIN?\r\n", response, sizeof(response), 1000) == ESP_OK) {
        if (strstr(response, "READY")) {
            ESP_LOGI(TAG, "SIM already unlocked");
            return ESP_OK;
        } else if (strstr(response, "SIM PIN")) {
            ESP_LOGI(TAG, "SIM locked, sending PIN");
        } else {
            ESP_LOGE(TAG, "Unexpected response: %s", response);
            return ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "Failed to get SIM status");
        return ESP_FAIL;
    }

    // 3. Sending command to unlock SIM if needed
    snprintf(cmd, sizeof(cmd), "AT+CPIN=\"%s\"\r\n", pin);

    // Clear input and output buffer and small delay
    gsm_uart_flush_and_delay();
    if (gsm_send_at_cmd(cmd, response, sizeof(response), 5000) == ESP_OK && strstr(response, "READY")) {
        ESP_LOGI(TAG, "PIN accepted, SIM unlocked: %s", response);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to unlock SIM: %s", response);
        return ESP_FAIL;
    }
}

static esp_err_t gsm_check_network_registration(void)
{
    char response[BUF_SIZE];
    int retries = 5; // Количество попыток проверки регистрации

    while (retries--) {
        gsm_uart_flush_and_delay();
        // Отправляем команду для проверки регистрации сети
        esp_err_t ret = gsm_send_at_cmd("AT+CREG?\r\n", response, sizeof(response), 1000);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Network registration response: %s", response);
            if (strstr(response, "0,1")) {
                ESP_LOGI(TAG, "Network successfully registered.");
                return ESP_OK;
            }
        } else {
            ESP_LOGE(TAG, "Failed to get network registration status.");
        }
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    ESP_LOGE(TAG, "Network registration failed after retries.");
    return ESP_FAIL;
}



/*--------------------------------------------------------------------------------*/
/*----------------------Initialisation / deInitialisation ------------------------*/
/*--------------------------------------------------------------------------------*/

// Construct a task for GSM module
static void gsm_uart_task(void *arg)
{
    if (gsm_set_pin("1234") == ESP_OK) {
        ESP_LOGI(TAG, "SIM unlocked successfully.");
        
        // Проверяем регистрацию сети
        if (gsm_check_network_registration() == ESP_OK) {
            ESP_LOGI(TAG, "GSM module is fully registered and ready.");
        } else {
            ESP_LOGE(TAG, "Network registration failed.");
        }
    } else {
        ESP_LOGE(TAG, "Failed to unlock SIM.");
    }

    while (1) {
        // Остальная логика работы модуля...
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

    vTaskDelay(2000 / portTICK_PERIOD_MS);
    

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



