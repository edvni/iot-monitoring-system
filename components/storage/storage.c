#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "storage.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

#define SECONDS_PER_DAY  86400      // 24 hours * 60 minutes * 60 second

static const char *TAG = "STORAGE";
static nvs_handle_t my_nvs_handle;

esp_err_t storage_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = nvs_open("storage", NVS_READWRITE, &my_nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle!");
    }
    
    return ret;
}

uint32_t storage_get_boot_count(void) {
    uint32_t boot_count = 0;
    esp_err_t ret = nvs_get_u32(my_nvs_handle, "boot_count", &boot_count);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Boot count not found, starting from 0");
        boot_count = 0;
    }
    return boot_count;
}

esp_err_t storage_increment_boot_count(void) {
    uint32_t boot_count = storage_get_boot_count();
    boot_count++;
    
    esp_err_t ret = nvs_set_u32(my_nvs_handle, "boot_count", boot_count);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error saving boot count!");
        return ret;
    }
    
    ret = nvs_commit(my_nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error committing NVS changes!");
    }
    
    return ret;
}

esp_err_t storage_check_and_reset_counter(uint32_t sleep_time) {
    uint32_t boot_count = storage_get_boot_count();
    
    if (boot_count * sleep_time >= SECONDS_PER_DAY) {
        ESP_LOGI(TAG, "24 hours passed, resetting counter");
        boot_count = 0;
        esp_err_t ret = nvs_set_u32(my_nvs_handle, "boot_count", boot_count);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Error resetting boot count!");
            return ret;
        }
        ret = nvs_commit(my_nvs_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Error committing NVS changes!");
            return ret;
        }
    }
    
    return ESP_OK;
}