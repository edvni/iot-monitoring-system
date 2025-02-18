#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "storage.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_spiffs.h" 
#include "json_helper.h"
#include "cJSON.h"
#include <unistd.h>
#include <sys/stat.h>
#include "esp_task_wdt.h" 

#define SECONDS_PER_DAY     86400    // 24 hours * 60 minutes * 60 second
#define MEASUREMENTS_FILE   "/spiffs/measurements.json"
#define WDT_TIMEOUT_LONG    30000    // 30 seconds for initialization
#define WDT_TIMEOUT_SHORT   5000     // 5 seconds for normal operation

static const char *TAG = "STORAGE";
static nvs_handle_t my_nvs_handle;

esp_err_t storage_init(void) {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // Use ret to store the result of erasing the NVS
        ret = nvs_flash_erase();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Error erasing NVS flash!");
            return ret;
        }
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error initializing NVS!");
        return ret;
    }
 
    ret = nvs_open("storage", NVS_READWRITE, &my_nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle!");
        return ret;
    }
 
    // Initialize SPIFFS
    const esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true
    };
    
    ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        // Close NVS handle before returning error
        nvs_close(my_nvs_handle);
        ESP_LOGE(TAG, "Failed to mount SPIFFS (%s)", esp_err_to_name(ret));
        return ret;
    }
 
    // Check SPIFFS status
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SPIFFS: total: %d, used: %d", total, used);
    } else {
        // Close NVS handle before returning error
        nvs_close(my_nvs_handle);
        return ret;
    }
 
    return ESP_OK;
 }
/*----------------------------------------------------------------------------------------------*/
/*----------------------------NVS operations for boot count-------------------------------------*/
/*----------------------------------------------------------------------------------------------*/
uint32_t storage_get_boot_count(void) {
   uint32_t boot_count = 0;
   if (nvs_get_u32(my_nvs_handle, "boot_count", &boot_count) == ESP_ERR_NVS_NOT_FOUND) {
       ESP_LOGI(TAG, "Boot count not found, starting from 0");
   }
   return boot_count;
}

esp_err_t storage_increment_boot_count(void) {
   uint32_t boot_count = storage_get_boot_count() + 1;
   esp_err_t ret = nvs_set_u32(my_nvs_handle, "boot_count", boot_count);
   return (ret != ESP_OK) ? ret : nvs_commit(my_nvs_handle);
}

esp_err_t storage_check_and_reset_counter(uint32_t sleep_time) {
   uint32_t boot_count = storage_get_boot_count();
   
   if (boot_count * sleep_time >= SECONDS_PER_DAY) {
   //if (boot_count  >= 5) {
       ESP_LOGI(TAG, "24 hours passed, resetting counter");
       esp_err_t ret = nvs_set_u32(my_nvs_handle, "boot_count", 0);
       return (ret != ESP_OK) ? ret : nvs_commit(my_nvs_handle);
   }
   return ESP_OK;
}

/*----------------------------------------------------------------------------------------------*/
/*----------------------------SPIFFS operations for measurements--------------------------------*/ 
/*----------------------------------------------------------------------------------------------*/

// Check SPIFFS status
static bool check_spiffs_status(void) {
   if (!esp_spiffs_mounted("storage")) {
       ESP_LOGE(TAG, "SPIFFS is not mounted");
       return false;
   }

   size_t total = 0, used = 0;
   if (esp_spiffs_info("storage", &total, &used) == ESP_OK && 
       used > (total * 0.9)) {
       esp_spiffs_gc("storage", 4096);
   }
   
   return true;
}
esp_err_t storage_save_measurement(ruuvi_measurement_t *measurement) {
    if (!check_spiffs_status()) {
        return ESP_ERR_INVALID_STATE;
    }

    // Read existing measurements
    cJSON *measurements_array = NULL;
    FILE* f = fopen(MEASUREMENTS_FILE, "r");
    
    if (f != NULL) {
        // Определяем размер файла
        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        fseek(f, 0, SEEK_SET);

        if (fsize > 0) {  // Проверяем, что файл не пустой
            char *json_str = malloc(fsize + 1);
            if (json_str != NULL) {
                size_t read_size = fread(json_str, 1, fsize, f);
                if (read_size == (size_t)fsize) {
                    json_str[fsize] = '\0';
                    measurements_array = cJSON_Parse(json_str);
                }
                free(json_str);
            }
        }
        fclose(f);
    }

    // Если массив не существует или не удалось прочитать, создаем новый
    if (measurements_array == NULL) {
        measurements_array = cJSON_CreateArray();
        if (measurements_array == NULL) {
            ESP_LOGE(TAG, "Failed to create measurements array");
            return ESP_ERR_NO_MEM;
        }
    }

    // Add new measurement
    cJSON *measurement_obj = json_helper_create_measurement_object(measurement);
    if (measurement_obj == NULL) {
        ESP_LOGE(TAG, "Failed to create measurement object");
        cJSON_Delete(measurements_array);
        return ESP_FAIL;
    }

    if (!cJSON_AddItemToArray(measurements_array, measurement_obj)) {
        ESP_LOGE(TAG, "Failed to add measurement to array");
        cJSON_Delete(measurement_obj);
        cJSON_Delete(measurements_array);
        return ESP_FAIL;
    }

    // Save updated array
    char *new_json_str = cJSON_PrintUnformatted(measurements_array);  // Используем неформатированный вывод
    cJSON_Delete(measurements_array);

    if (new_json_str == NULL) {
        ESP_LOGE(TAG, "Failed to print JSON string");
        return ESP_ERR_NO_MEM;
    }

    // Записываем в файл
    f = fopen(MEASUREMENTS_FILE, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        free(new_json_str);
        return ESP_FAIL;
    }

    fprintf(f, "%s", new_json_str);
    fclose(f);
    free(new_json_str);

    return ESP_OK;
}

// Get measurements from SPIFFS

char* storage_get_measurements(void) {
   if (!check_spiffs_status()) {
       return NULL;
   }

   FILE* f = fopen(MEASUREMENTS_FILE, "r");
   if (f == NULL) {
       ESP_LOGE(TAG, "No measurements file found");
       return NULL;
   }

   fseek(f, 0, SEEK_END);
   long fsize = ftell(f);
   fseek(f, 0, SEEK_SET);

   char *json_str = malloc(fsize + 1);
   if (json_str == NULL) {
       fclose(f);
       return NULL;
   }

   if (fread(json_str, 1, fsize, f) != (size_t)fsize) {
       free(json_str);
       fclose(f);
       return NULL;
   }

   fclose(f);
   json_str[fsize] = '\0';
   return json_str;
}

// Clear measurements from SPIFFS
esp_err_t storage_clear_measurements(void) {
   return check_spiffs_status() ? unlink(MEASUREMENTS_FILE) : ESP_ERR_INVALID_STATE;
}





/*----------------------------------------------------------------------------------------------*/
/*----------------------------First boot flag operations for GSM--------------------------------*/
/*----------------------------------------------------------------------------------------------*/

bool storage_is_first_boot(void) {
    uint8_t first_boot = 1;  // By default, we assume this is the first boot
    esp_err_t err = nvs_get_u8(my_nvs_handle, "first_boot", &first_boot);
    
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "First boot flag not found, assuming first boot");
        return true;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error reading first boot flag");
        return true;
    }
    
    return first_boot == 1;
}

esp_err_t storage_set_first_boot_completed(void) {
    esp_err_t err = nvs_set_u8(my_nvs_handle, "first_boot", 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error setting first boot flag");
        return err;
    }
    
    err = nvs_commit(my_nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error committing first boot flag");
        return err;
    }
    
    return ESP_OK;
}