#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "power_management.h"
#include "esp_pm.h"
#include "esp_log.h"

static const char *TAG = "POWER_MANAGEMENT";

esp_err_t power_management_init(void) {
    #ifdef CONFIG_PM_ENABLE
        esp_pm_config_t pm_config = {
            .max_freq_mhz = 80,        
            .min_freq_mhz = 40,        
            .light_sleep_enable = false  
        };
        
        esp_err_t ret = esp_pm_configure(&pm_config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Power management configuration failed");
            return ret;
        }
        ESP_LOGI(TAG, "Power management configured with max frequency: %d MHz", pm_config.max_freq_mhz);
    #endif

    return ESP_OK;
}