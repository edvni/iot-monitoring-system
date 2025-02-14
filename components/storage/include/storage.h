#ifndef STORAGE_H
#define STORAGE_H

#include "esp_err.h"
#include <stdint.h>

// Инициализация хранилища
esp_err_t storage_init(void);

// Получение значения счетчика загрузок
uint32_t storage_get_boot_count(void);

// Увеличение и сохранение счетчика загрузок
esp_err_t storage_increment_boot_count(void);

#endif // STORAGE_H