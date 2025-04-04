#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Информация о батарее
 */
typedef struct {
    uint32_t voltage_mv;    // Напряжение в милливольтах
    int level;              // Уровень заряда в процентах (0-100%)
} battery_info_t;

/**
 * @brief Инициализация модуля мониторинга батареи
 * 
 * @return esp_err_t ESP_OK при успехе
 */
esp_err_t battery_monitor_init(void);

/**
 * @brief Чтение текущего состояния батареи
 * 
 * @param info Указатель на структуру для сохранения информации
 * @return esp_err_t ESP_OK при успехе
 */
esp_err_t battery_monitor_read(battery_info_t *info);

#ifdef __cplusplus
}
#endif 