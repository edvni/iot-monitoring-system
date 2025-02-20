#ifndef GSM_MODULE_H
#define GSM_MODULE_H

#include "esp_err.h"
#include "esp_netif.h"
#include "esp_modem_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GSM_UART_PORT_NUM      (2)
#define GSM_UART_TX_PIN        (26)
#define GSM_UART_RX_PIN        (27)
#define GSM_PWRKEY_PIN         (4)
#define GSM_RESET_PIN          (5)
#define GSM_POWER_PIN          (12)
#define BUF_SIZE               (2048)
#define GSM_PIN                "1234"
#define GSM_APN                "internet"

// Инициализация модема
esp_err_t gsm_module_init(void);


#ifdef __cplusplus
}
#endif

#endif // GSM_MODULE_H








// // gsm_module.h
// #ifndef GSM_MODULE_H
// #define GSM_MODULE_H

// #include "esp_err.h"
// #include "driver/uart.h"

// #define GSM_UART_PORT_NUM      (2)
// #define GSM_UART_TX_PIN        (26)
// #define GSM_UART_RX_PIN        (27)
// #define GSM_PWRKEY_PIN         (4)
// #define GSM_RESET_PIN          (5)
// #define GSM_POWER_PIN          (12)
// #define BUF_SIZE               (2048)
// #define GSM_PIN                "1234"


// esp_err_t gsm_module_init(void);
// esp_err_t gsm_module_deinit(void);
// esp_err_t gsm_wait_for_ready(void);
// esp_err_t gsm_send_at_cmd(const char *cmd, char *response, size_t response_size, uint32_t timeout_ms);
// esp_err_t gsm_set_pin(const char* pin);

// #endif  // GSM_MODULE_H