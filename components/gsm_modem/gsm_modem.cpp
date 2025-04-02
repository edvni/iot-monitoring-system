#include <cstdio>
#include <cstring>
#include "sdkconfig.h"
#include "esp_console.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_modem_types.hpp"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "cxx_include/esp_modem_dte.hpp"
#include "esp_modem_config.h"
#include "cxx_include/esp_modem_api.hpp"
#include "my_module_dce.hpp"
#include "main_config.h"
#include "gsm_modem.h"

#include "esp_netif_ip_addr.h"

static const char *TAG = "gsm_modem";
using namespace esp_modem;

// Global variables 
static std::string accumulated_response;
static bool response_completed = false;
static bool modem_initialized = false;
static bool system_initialized = false;
//static ModemStatus modem_status;

// Declare static pointers to store modem objects
static std::unique_ptr<Shiny::DCE> s_dce;
static esp_netif_t *s_esp_netif = nullptr;

#define GPIO_OUTPUT_POWER     (gpio_num_t)MODEM_POWER_PIN
#define GPIO_OUTPUT_PWRKEY    (gpio_num_t)MODEM_PWRKEY_PIN
#define GPIO_OUTPUT_RESET     MODEM_RESET_PIN
#define MODEM_RESET_LEVEL       1
#define INPUT                   0x01
#define OUTPUT                  0x03     



static esp_err_t initialize_system_once() {
    if (!system_initialized) {
        ESP_ERROR_CHECK(nvs_flash_init());
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        system_initialized = true;
    }
    return ESP_OK;
}
// implementation of a function for managing pins

void pinMode(uint32_t gpio, uint8_t mode)
{
    gpio_config_t config;
    memset(&config, 0, sizeof(config));
    config.pin_bit_mask = 1ULL << gpio;
    switch (mode) {
    case INPUT:
        config.mode = GPIO_MODE_INPUT;
        break;
    case OUTPUT:
        config.mode = GPIO_MODE_OUTPUT;
        break;
    }
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&config));
}

void digitalWrite(uint32_t gpio, uint8_t level)
{
    gpio_set_level((gpio_num_t )gpio, level);
}

void config_modem_gpios()
{
    pinMode(MODEM_POWER_PIN,OUTPUT);
    gpio_set_level((gpio_num_t)MODEM_POWER_PIN, 1);
}

void power_on_modem()
{

#ifdef MODEM_RESET_PIN
    // Set modem reset pin ,reset modem
    pinMode(MODEM_RESET_PIN,OUTPUT);
    gpio_set_level((gpio_num_t)MODEM_RESET_PIN, !MODEM_RESET_LEVEL);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level((gpio_num_t)MODEM_RESET_PIN, MODEM_RESET_LEVEL);
    vTaskDelay(pdMS_TO_TICKS(2600));
    gpio_set_level((gpio_num_t)MODEM_RESET_PIN, !MODEM_RESET_LEVEL);
#endif

    pinMode(MODEM_PWRKEY_PIN,OUTPUT);
    gpio_set_level((gpio_num_t)MODEM_PWRKEY_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level((gpio_num_t)MODEM_PWRKEY_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level((gpio_num_t)MODEM_PWRKEY_PIN, 0);


}

// Helper function for handling responses from the modem

static command_result process_line(uint8_t *data, size_t len) 
{
    if (len == 0) return command_result::TIMEOUT;
    
    std::string response((char*)data, len);
    ESP_LOGI(TAG, "Received: %s", response.c_str());
    
    accumulated_response += response;
    
    // Если получили OK и нет ERROR - считаем что команда выполнена успешно
    if (accumulated_response.find("OK") != std::string::npos && 
        accumulated_response.find("ERROR") == std::string::npos) {
        response_completed = true;
        return command_result::OK;
    }

    // Если получили ERROR - команда завершилась с ошибкой
    if (accumulated_response.find("ERROR") != std::string::npos) {
        response_completed = true;
        return command_result::FAIL;
    }
    
    // Все остальные ответы считаем промежуточными
    return command_result::TIMEOUT;
}

// Modem configuration

static esp_err_t configure_modem(void)
{

    initialize_system_once();

    config_modem_gpios();

    power_on_modem();
    
    // DCE configuration
    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(MODEM_PPP_APN);
    esp_netif_config_t ppp_netif_config = ESP_NETIF_DEFAULT_PPP();
    s_esp_netif = esp_netif_new(&ppp_netif_config);
    if (!s_esp_netif) {
        ESP_LOGE(TAG, "Failed to create netif");
        return ESP_FAIL;
    }

    // UART configuration
    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    dte_config.uart_config.tx_io_num = MODEM_UART_TX_PIN;
    dte_config.uart_config.rx_io_num = MODEM_UART_RX_PIN;
    dte_config.uart_config.rts_io_num = MODEM_UART_RTS_PIN;
    dte_config.uart_config.cts_io_num = MODEM_UART_CTS_PIN;
    dte_config.uart_config.flow_control = ESP_MODEM_FLOW_CONTROL_NONE;
    dte_config.uart_config.baud_rate = 115200;
    dte_config.uart_config.rx_buffer_size = 1024 * 2;
    dte_config.uart_config.tx_buffer_size = 1024 * 2;
    auto uart_dte = create_uart_dte(&dte_config);

    // DCE creation
    ESP_LOGI(TAG, "Initializing modem...");
    s_dce = create_shiny_dce(&dce_config, uart_dte, s_esp_netif);
    if (!s_dce) {
        ESP_LOGE(TAG, "Failed to create DCE");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Modem initialization completed successfully");
    return ESP_OK;

}

// Function template for sending AT command
static bool send_at_command(std::unique_ptr<Shiny::DCE>& dce, const std::string& command, uint32_t timeout = 5000) 
{
    ESP_LOGI(TAG, "Sending command: %s", command.c_str());
    
    for (int retry = 0; retry < 5; retry++) {
        accumulated_response.clear();
        response_completed = false;
        
        auto result = dce->command(command + "\r", process_line, timeout);
        if (result == command_result::OK && response_completed) {
            return true;
        }
        
        ESP_LOGW(TAG, "Command failed (attempt %d/5)", retry + 1);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
    
    return false;
}

// Checking modem readiness

static bool wait_for_modem_ready(std::unique_ptr<Shiny::DCE>& dce)
{
    int retries = 10;
    bool power_down_detected = false;
    
    while (retries--) {
        accumulated_response.clear();
        response_completed = false;
        
        if (send_at_command(dce, "AT", 1000)) {
            ESP_LOGI(TAG, "Modem responded to AT command");
            
            // Checking if the response was POWER DOWN
            if (accumulated_response.find("POWER DOWN") != std::string::npos) {
                ESP_LOGI(TAG, "Power down state detected, waiting for full initialization");
                power_down_detected = true;
                vTaskDelay(pdMS_TO_TICKS(5000));  // Wait 5 seconds after POWER DOWN
                continue;  // We continue to check readiness
            }
            
            // If there was a POWER DOWN, we give additional time for SIM initialization
            if (power_down_detected) {
                ESP_LOGI(TAG, "Additional delay after power down");
                vTaskDelay(pdMS_TO_TICKS(5000));
            }
            
            return true;
        }
        
        if (accumulated_response.find("*ATREADY") != std::string::npos) {
            ESP_LOGI(TAG, "Modem indicated ready state");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    return false;
}

// Modem initialization (here you can specify a sequence of AT commands to check the modem status)

static bool start_checking(std::unique_ptr<Shiny::DCE>& dce)
{
    if (!wait_for_modem_ready(dce)) {
        ESP_LOGE(TAG, "Modem not responding");

        return false;
    }

    const std::vector<std::pair<std::string, std::string>> init_sequence = {
        {"AT+CPIN?", "Check SIM status"},
        {"AT+CSQ", "Check signal quality"}
    };

    for (const auto& cmd : init_sequence) {
        ESP_LOGI(TAG, "--- %s ---", cmd.second.c_str());
        if (!send_at_command(dce, cmd.first)) {
            ESP_LOGE(TAG, "Failed: %s", cmd.second.c_str());
            return false;
        }

        if (cmd.first == "AT+CPIN?") {
            if (accumulated_response.find("READY") != std::string::npos) {
                ESP_LOGI(TAG, "SIM is ready");
            } else if (accumulated_response.find("SIM PIN") != std::string::npos) {
                ESP_LOGI(TAG, "SIM requires PIN, entering PIN code");
                if (!send_at_command(dce, "AT+CPIN=" MODEM_SIM_PIN, 10000)) {
                    ESP_LOGE(TAG, "Failed to enter PIN code");
                    return false;
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    
    return true;
}

// Switching the modem to data transfer mode

static command_result enable_data_mode(std::unique_ptr<Shiny::DCE>& s_dce)
{
    // Create a PDP context with the required parameters
    esp_modem::PdpContext pdp_context("internet");
    if (s_dce->set_pdp_context(pdp_context) != command_result::OK) {
        ESP_LOGE(TAG, "Failed to set PDP context");
        return command_result::FAIL;
    }

    // Switching the modem to data transfer mode
    if (!s_dce->set_mode(esp_modem::modem_mode::DATA_MODE)) {
        ESP_LOGE(TAG, "Failed to enable data mode");
        return command_result::FAIL;
    }

    ESP_LOGI(TAG, "Data mode enabled successfully.");
    
    // Waiting to receive IP address
    int retry = 0;
    esp_netif_ip_info_t ip_info;
    while(retry < 10) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_netif_get_ip_info(s_esp_netif, &ip_info);
        if(ip_info.ip.addr != 0) {
            ESP_LOGI(TAG, "IP received: " IPSTR, IP2STR(&ip_info.ip));
            return command_result::OK;
        }
        retry++;
    }
    
    ESP_LOGE(TAG, "Failed to get IP address");
    return command_result::FAIL;
}


extern "C" {
    // Modem initialization
    esp_err_t gsm_modem_init(void)
    {
        if (modem_initialized) {
            return ESP_ERR_INVALID_STATE;
        }

        ESP_LOGI(TAG, "Initializing GSM modem");

        // Modem configuration
        if (configure_modem() != ESP_OK) {
            ESP_LOGE(TAG, "Modem initializing failed: Failed to configure modem");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "Modem configured successfully");
       
    
        // We give the modem time for initial initialization
        vTaskDelay(pdMS_TO_TICKS(5000));
    
        // Initial checks
        if (!start_checking(s_dce)) {
            ESP_LOGE(TAG, "Modem initializing failed: Start checking failed");
            return ESP_FAIL;
        }

        // Enabling data transfer mode
        if (enable_data_mode(s_dce) != command_result::OK) {	
            ESP_LOGE(TAG, "Modem initializing failed: Failed to enable data mode");
            return ESP_FAIL;
        }
    
        ESP_LOGI(TAG, "Modem initialization completed successfully");
        return ESP_OK;
    }

    // Modem deinitialization

    esp_err_t gsm_modem_deinit(void)
    {
        if (!s_dce) {
            return ESP_ERR_INVALID_STATE;
        }
        ESP_LOGI(TAG, "Deinitializing GSM modem");

        // Properly turning off the modem before destroying it
        s_dce->set_mode(esp_modem::modem_mode::COMMAND_MODE);
        s_dce->power_down();
        
        // Destroying objects in the correct order
        s_dce.reset();  // Destroys DCE and DTE
        
        // Freeing netif

        if (s_esp_netif) {
            esp_netif_destroy(s_esp_netif);
            s_esp_netif = nullptr;
        }

        ESP_LOGI(TAG, "GSM modem deinitialized successfully");
    
        modem_initialized = false;
        return ESP_OK;
    }

    // Getting battery charge status
    
    esp_err_t gsm_modem_get_battery_status(battery_status_t* status) {
        if (!status || !s_dce) {
            return ESP_ERR_INVALID_ARG;
        }
    
        int voltage, bcs, bcl;
        if (s_dce->get_battery_status(voltage, bcs, bcl) == command_result::OK) {
            status->voltage = voltage;
            status->charge_status = bcs;
            status->level = bcl;
            return ESP_OK;
        }
        
        return ESP_FAIL;
    }

    esp_err_t modem_power_off(void)
    {
        ESP_LOGI(TAG, "Power off the modem");

        // Putting the modem into command mode
        auto result = s_dce->power_down();
        if (result == command_result::FAIL) {
            ESP_LOGE(TAG, "Failed to power down modem");
            return ESP_FAIL;
        }
        
        // Power off the modem  
        gpio_set_level((gpio_num_t)GPIO_OUTPUT_POWER, 0);
        gpio_set_level((gpio_num_t)GPIO_OUTPUT_PWRKEY, 0);
        
        return ESP_OK;
    }
    
    
} // extern "C"