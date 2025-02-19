// gsm_module.c 
#include "gsm_module.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "GSM_MODULE";
static QueueHandle_t gsm_uart_queue;
static TaskHandle_t gsm_event_task_handle = NULL;
static TaskHandle_t gsm_task_handle = NULL;
static char *last_command = NULL;

static bool wait_for_response = false;
static char current_response[BUF_SIZE];

// // Clear input and output buffer and small delay
static void gsm_uart_flush_and_delay(void)
{
    uart_flush_input(GSM_UART_PORT_NUM);
    uart_flush(GSM_UART_PORT_NUM);
    vTaskDelay(100 / portTICK_PERIOD_MS);
}

esp_err_t gsm_set_pin(const char* pin)
{
    
    char response[BUF_SIZE];
    char cmd[32];

    // Clear input and output buffer and small delay
    gsm_uart_flush_and_delay();

    // Test AT command to verify communication
    if (gsm_send_at_cmd("AT+CREG=0\r\n", response, sizeof(response), 1000) != ESP_OK) {
        ESP_LOGE(TAG, "No response from modem");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Modem responded to AT command");

    // Clear input and output buffer and small delay
    gsm_uart_flush_and_delay();

    // 1. Turning off echo
    if (gsm_send_at_cmd("ATE0\r\n", response, sizeof(response), 500) == ESP_OK) {
        ESP_LOGI(TAG, "Echo disabled: %s", response);
    } else {
        ESP_LOGW(TAG, "Failed to disable echo, proceeding anyway...");
    }

    // 2. Checking SIM status
    if (gsm_send_at_cmd("AT+CPIN?\r\n", response, sizeof(response), 500) == ESP_OK) {
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
    if (gsm_send_at_cmd(cmd, response, sizeof(response), 3000) == ESP_OK) {
        if (strstr(response, "OK")) {
            ESP_LOGI(TAG, "PIN accepted, SIM unlocked");
            return ESP_OK;
        }
    }

    ESP_LOGE(TAG, "Failed to unlock SIM");
    return ESP_FAIL;
}

esp_err_t gsm_registration(void)
{
    char response[BUF_SIZE];
    const int MAX_RETRIES = 10;
    const int RETRY_DELAY_MS = 3000;

    // Check registration status with retries
    for (int retry = 0; retry < MAX_RETRIES; retry++) {
        gsm_uart_flush_and_delay();
        if (gsm_send_at_cmd("AT+CREG?\r\n", response, sizeof(response), 9000) == ESP_OK) {
            if (strstr(response, "+CREG:") && 
               (strstr(response, "0,1") || strstr(response, "0,5"))) {
                ESP_LOGI(TAG, "Registration is done: %s", response);
                return ESP_OK;
            }
        }

        ESP_LOGW(TAG, "Registration attempt %d failed, retrying in %d ms...", 
                 retry + 1, RETRY_DELAY_MS);
        vTaskDelay(RETRY_DELAY_MS / portTICK_PERIOD_MS);
    }

    ESP_LOGE(TAG, "Failed to register after %d attempts", MAX_RETRIES);
    return ESP_FAIL;
}


// Таск для обработки UART событий
static void gsm_uart_event_task(void *pvParameters)
{
    uart_event_t event;
    uint8_t* dtmp = (uint8_t*) malloc(BUF_SIZE);
    
    for(;;) {
        if(xQueueReceive(gsm_uart_queue, (void *)&event, (TickType_t)portMAX_DELAY)) {

            memset(dtmp, 0, BUF_SIZE);

            switch(event.type) {

                case UART_DATA:
                ESP_LOGI(TAG, "[UART DATA]: %d", event.size);
                if (uart_read_bytes(GSM_UART_PORT_NUM, dtmp, event.size, portMAX_DELAY) > 0) {
                    ESP_LOGI(TAG, "Read data (ASCII): %s", dtmp);
                    ESP_LOG_BUFFER_HEX(TAG, dtmp, event.size);
                    
                    if (wait_for_response && last_command) {
                        // Проверяем, относится ли ответ к последней команде
                        if (strstr((char*)dtmp, last_command) || 
                            (strstr((char*)dtmp, "OK") || strstr((char*)dtmp, "ERROR"))) {
                            strncat(current_response, (char*)dtmp, BUF_SIZE - strlen(current_response) - 1);
                            
                            if (strstr(current_response, "OK") || strstr(current_response, "ERROR")) {
                                wait_for_response = false;
                                free(last_command);
                                last_command = NULL;
                            }
                        }
                        // Иначе это асинхронное сообщение - игнорируем
                    }
                }
                break;
                    
                case UART_FIFO_OVF:
                    ESP_LOGE(TAG, "HW FIFO Overflow");
                    // uart_flush_input(GSM_UART_PORT_NUM);
                    gsm_uart_flush_and_delay();
                    xQueueReset(gsm_uart_queue);
                    break;
                    
                case UART_BUFFER_FULL:
                    ESP_LOGE(TAG, "Ring Buffer Full");
                    //uart_flush_input(GSM_UART_PORT_NUM);
                    gsm_uart_flush_and_delay();
                    xQueueReset(gsm_uart_queue);
                    break;
                    
                case UART_PATTERN_DET:
                    size_t buffered_size;
                    uart_get_buffered_data_len(GSM_UART_PORT_NUM, &buffered_size);
                    int pos = uart_pattern_pop_pos(GSM_UART_PORT_NUM);
                    ESP_LOGI(TAG, "Pattern detected at position %d", pos);
                    break;
                    
                default:
                    ESP_LOGI(TAG, "Other event type: %d", event.type);
                    break;
            }
        }
    }
    free(dtmp);
    vTaskDelete(NULL);
}

// Основной таск GSM операций
static void gsm_uart_task(void *arg)
{
    if (gsm_set_pin("1234") == ESP_OK) {
        ESP_LOGI(TAG, "SIM works successfully.");

        // Отключаем PPP
        char response[BUF_SIZE];
        if (gsm_send_at_cmd("AT+CGATT=0\r\n", response, sizeof(response), 3000) == ESP_OK) {
            ESP_LOGI(TAG, "PPP detached");
        }

        vTaskDelay(10000 / portTICK_PERIOD_MS);

        if (gsm_registration() == ESP_OK) {
            ESP_LOGI(TAG, "GSM module is fully registered and ready.");
        } else {
            ESP_LOGE(TAG, "Network registration failed.");
        }
    } else {
        ESP_LOGE(TAG, "Failed to unlock SIM.");
    }

    while (1) {
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
        .source_clk = UART_SCLK_APB,
    };

    // Install UART driver and get event queue
    ESP_ERROR_CHECK(uart_driver_install(GSM_UART_PORT_NUM, BUF_SIZE * 4, BUF_SIZE * 4, 50, &gsm_uart_queue, 0));
    ESP_ERROR_CHECK(uart_param_config(GSM_UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(GSM_UART_PORT_NUM, GSM_UART_TX_PIN, GSM_UART_RX_PIN, 
                                UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    uart_set_rx_full_threshold(GSM_UART_PORT_NUM, 64);
    uart_set_tx_empty_threshold(GSM_UART_PORT_NUM, 10);
    uart_set_rx_timeout(GSM_UART_PORT_NUM, 2);

    // Set pattern detection for "OK" and "ERROR" responses
    uart_enable_pattern_det_baud_intr(GSM_UART_PORT_NUM, 'O', 2, 9, 0, 0);
    uart_pattern_queue_reset(GSM_UART_PORT_NUM, 20);

    // Power sequence
    gpio_set_level(GSM_POWER_PIN, 1);
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    gpio_set_level(GSM_PWRKEY_PIN, 0);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    gpio_set_level(GSM_PWRKEY_PIN, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    gpio_set_level(GSM_PWRKEY_PIN, 0);
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    // Create UART event handling task first
    xTaskCreate(gsm_uart_event_task, "uart_event_task", 3072, NULL, 12, &gsm_event_task_handle);
    
    // Then create main GSM task
    xTaskCreate(gsm_uart_task, "gsm_uart_task", 8192, NULL, 10, &gsm_task_handle);

    return ESP_OK;
}

esp_err_t gsm_module_deinit(void)
{
    if (gsm_event_task_handle) {
        vTaskDelete(gsm_event_task_handle);
    }
    if (gsm_task_handle) {
        vTaskDelete(gsm_task_handle);
    }
    return uart_driver_delete(GSM_UART_PORT_NUM);
}


// Модифицированная функция отправки AT команд
esp_err_t gsm_send_at_cmd(const char* cmd, char* response, size_t response_size, uint32_t timeout_ms)
{
    if (!cmd) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(current_response, 0, BUF_SIZE);
    // uart_flush(GSM_UART_PORT_NUM);
    gsm_uart_flush_and_delay();

    wait_for_response = true;

    int written = uart_write_bytes(GSM_UART_PORT_NUM, cmd, strlen(cmd));
    ESP_LOGI(TAG, "Sent: %s (bytes written: %d)", cmd, written);

    uart_wait_tx_done(GSM_UART_PORT_NUM, pdMS_TO_TICKS(100));

    last_command = strdup(cmd);

    // Wait for response using event task
    TickType_t start_time = xTaskGetTickCount();
    while (wait_for_response) {
        if ((xTaskGetTickCount() - start_time) >= pdMS_TO_TICKS(timeout_ms)) {
            wait_for_response = false;
            ESP_LOGW(TAG, "Command timeout");
            return ESP_FAIL;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (response && response_size > 0) {
        strncpy(response, current_response, response_size - 1);
        response[response_size - 1] = '\0';
    }

    return (strstr(current_response, "OK") != NULL) ? ESP_OK : ESP_FAIL;
}