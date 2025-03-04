// main_config.h

#ifndef MODEM_CONFIG_H
#define MODEM_CONFIG_H

// UART Pins
#define MODEM_UART_TX_PIN         26
#define MODEM_UART_RX_PIN         27
#define MODEM_UART_RTS_PIN        27
#define MODEM_UART_CTS_PIN        23

// Power Key Pin
#define MODEM_PWRKEY_PIN          4
#define MODEM_POWER_PIN           12
#define MODEM_RESET_PIN           5
#define MODEM_BAT_ADC_PIN         35

// UART Configuration
#define MODEM_UART_BAUD_RATE      115200
#define MODEM_UART_RX_BUFFER_SIZE (1024)
#define MODEM_UART_TX_BUFFER_SIZE (512)
#define MODEM_UART_EVENT_QUEUE_SIZE 30

// Task Configuration
#define MODEM_UART_EVENT_TASK_STACK_SIZE (2048)
#define MODEM_UART_EVENT_TASK_PRIORITY   10

// Flow Control
#define MODEM_FLOW_CONTROL ESP_MODEM_FLOW_CONTROL_HW  // или ESP_MODEM_FLOW_CONTROL_NONE

// PPP Configuration
#define MODEM_PPP_APN    "internet"  // APN for your operator
#define MODEM_PPP_USER   ""          // username if required
#define MODEM_PPP_PASS   ""          // password if required

// SIM PIN Configuration
#define MODEM_NEED_SIM_PIN    1       // 1 if SIM card has PIN or 0 if not
#define MODEM_SIM_PIN         "1234"  // your PIN code

// Timeouts (in milliseconds)
#define MODEM_CONNECT_TIMEOUT_MS (60000)  // timeout for connection

#endif // MAIN_CONFIG_H