#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
//#define LOG_LOCAL_LEVEL ESP_LOG_NONE
#include "storage.h"
#include "sensors.h"
#include "system_state.h"
#include "time_manager.h"
#include "json_helper.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_spiffs.h" 
#include "cJSON.h"
#include <unistd.h>
#include <sys/stat.h>
#include "esp_task_wdt.h" 
#include "esp_timer.h"
#include <string.h>
#include <math.h>

#define SECONDS_PER_DAY     86400    // 24 hours * 60 minutes * 60 second
#define MEASUREMENTS_FILE   "/spiffs/measurements.json"
#define FIRESTORE_MEASUREMENTS_FILE "/spiffs/firestore_measurements.json"
#define WDT_TIMEOUT_LONG    30000    // 30 seconds for initialization
#define WDT_TIMEOUT_SHORT   5000     // 5 seconds for normal operation
#define BOOT_COUNT_KEY "boot_count"
#define ERROR_FLAG_KEY "error_flag"
#define FIRST_BOOT_KEY "first_boot"
#define GSM_FIRST_BLOCK_KEY "gsm_first_block"
#define GSM_SECOND_BLOCK_KEY "gsm_second_block"
#define GSM_THIRD_BLOCK_KEY "gsm_third_block"
#define LAST_TRIGGER_TIME_KEY "last_trigger_time"

static const char *TAG = "STORAGE";
static nvs_handle_t my_nvs_handle;

esp_err_t storage_init(void) {
   // Initialize NVS
   esp_err_t ret = nvs_flash_init();
   if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
       ESP_ERROR_CHECK(nvs_flash_erase());
       ret = nvs_flash_init();
   }
   ESP_ERROR_CHECK(ret);

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
       ESP_LOGE(TAG, "Failed to mount SPIFFS (%s)", esp_err_to_name(ret));
       return ret;
   }

   // Check SPIFFS status
   size_t total = 0, used = 0;
   ret = esp_spiffs_info(conf.partition_label, &total, &used);
   if (ret == ESP_OK) {
       ESP_LOGI(TAG, "SPIFFS: total: %d, used: %d", total, used);
   }

   return ESP_OK;
}

// -------------------- NVS operations for boot count -------------------- //

// Get the boot count from NVS
uint32_t storage_get_boot_count(void) {
   uint32_t boot_count = 0;
   if (nvs_get_u32(my_nvs_handle, "boot_count", &boot_count) == ESP_ERR_NVS_NOT_FOUND) {
       ESP_LOGI(TAG, "Boot count not found, starting from 0");
   }
   return boot_count;
}

// Increment the boot count and save it to NVS
esp_err_t storage_increment_boot_count(void) {
   uint32_t boot_count = storage_get_boot_count() + 1;
   esp_err_t ret = nvs_set_u32(my_nvs_handle, "boot_count", boot_count);
   return (ret != ESP_OK) ? ret : nvs_commit(my_nvs_handle);
}

// Reset the boot count to 0
esp_err_t storage_reset_counter(void) {
   
    esp_err_t ret = nvs_set_u32(my_nvs_handle, "boot_count", 0);
    return (ret != ESP_OK) ? ret : nvs_commit(my_nvs_handle);

}

// -------------------- SPIFFS operations for measurements -------------------- //

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

// Save a measurement to SPIFFS
esp_err_t storage_save_measurement(ruuvi_measurement_t *measurement) {
   if (!check_spiffs_status()) {
       return ESP_ERR_INVALID_STATE;
   }

//    // 1. Сначала сохраняем в обычном формате для совместимости
//    ESP_LOGI(TAG, "Attempting to read file: %s", MEASUREMENTS_FILE);
//    struct stat st;
//    if (stat(MEASUREMENTS_FILE, &st) == 0) {
//        ESP_LOGI(TAG, "File exists, size: %ld bytes", st.st_size);
//    } else {
//        ESP_LOGI(TAG, "File does not exist, creating new");
//    }

//    // Read existing measurements
//    cJSON *measurements_array = NULL;
//    FILE* f = fopen(MEASUREMENTS_FILE, "r");
   
//    if (f != NULL) {
//        fseek(f, 0, SEEK_END);
//        long fsize = ftell(f);
//        fseek(f, 0, SEEK_SET);

//        char *json_str = malloc(fsize + 1);
//        if (json_str != NULL) {
//            if (fread(json_str, 1, fsize, f) == (size_t)fsize) {
//                json_str[fsize] = '\0';
//                measurements_array = cJSON_Parse(json_str);
//            }
//            free(json_str);
//        }
//        fclose(f);
//    }

//    if (measurements_array == NULL) {
//        measurements_array = cJSON_CreateArray();
//        if (measurements_array == NULL) {
//            return ESP_ERR_NO_MEM;
//        }
//    }

//    ESP_LOGI(TAG, "Array size before adding: %d", cJSON_GetArraySize(measurements_array));

//    // Add new measurement (using the standard format for compatibility)
//    cJSON *measurement_obj = json_helper_create_measurement_object(measurement);
//    if (measurement_obj == NULL || !cJSON_AddItemToArray(measurements_array, measurement_obj)) {
//        cJSON_Delete(measurements_array);
//        return ESP_FAIL;
//    }

//    ESP_LOGI(TAG, "Array size after adding: %d", cJSON_GetArraySize(measurements_array));

//    // Save updated array
//    char *new_json_str = cJSON_PrintUnformatted(measurements_array);
//    cJSON_Delete(measurements_array);

//    if (new_json_str == NULL) {
//        return ESP_ERR_NO_MEM;
//    }

//    f = fopen(MEASUREMENTS_FILE, "w");
//    if (f == NULL) {
//        free(new_json_str);
//        return ESP_FAIL;
//    }

//    fprintf(f, "%s", new_json_str);
//    fclose(f);
//    free(new_json_str);

   // 2. Теперь сохраняем в формате Firestore
   ESP_LOGI(TAG, "Saving in Firestore format to: %s", FIRESTORE_MEASUREMENTS_FILE);
   
   // Для Firestore нам нужно сохранить документ с полями
   cJSON *firestore_doc = NULL;
   cJSON *firestore_fields = NULL;
   cJSON *measurements_field = NULL;
   cJSON *array_value = NULL;
   cJSON *values_array = NULL;
   
   // Проверяем, существует ли уже файл с Firestore структурой
   FILE* f = fopen(FIRESTORE_MEASUREMENTS_FILE, "r");
   if (f != NULL) {
       // Если файл существует, загружаем структуру
       fseek(f, 0, SEEK_END);
       long fsize = ftell(f);
       fseek(f, 0, SEEK_SET);

       if (fsize > 0) {
           char *firestore_str = malloc(fsize + 1);
           if (firestore_str != NULL) {
               if (fread(firestore_str, 1, fsize, f) == (size_t)fsize) {
                   firestore_str[fsize] = '\0';
                   firestore_doc = cJSON_Parse(firestore_str);
               }
               free(firestore_str);
           }
       }
       fclose(f);
   }
   
   if (firestore_doc == NULL) {
       // Если файл не существует или пустой, создаем новую структуру
       firestore_doc = cJSON_CreateObject();
       firestore_fields = cJSON_CreateObject();
       
       // Добавляем tag_id (MAC адрес) как поле
       cJSON *tag_id_field = cJSON_CreateObject();
       cJSON_AddStringToObject(tag_id_field, "stringValue", measurement->mac_address);
       cJSON_AddItemToObject(firestore_fields, "tag_id", tag_id_field);
       
       // Добавляем текущую дату как поле
       char time_str[32];
       if (time_manager_get_formatted_time(time_str, sizeof(time_str)) != ESP_OK) {
           strcpy(time_str, "Time not available");
       }
       
       // Извлекаем только дату (первые 10 символов)
       char day[11] = {0}; // YYYY-MM-DD\0
       strncpy(day, time_str, 10);
       day[10] = '\0';
       
       cJSON *day_field = cJSON_CreateObject();
       cJSON_AddStringToObject(day_field, "stringValue", day);
       cJSON_AddItemToObject(firestore_fields, "day", day_field);
       
       // Создаем массив measurements
       measurements_field = cJSON_CreateObject();
       array_value = cJSON_CreateObject();
       values_array = cJSON_CreateArray();
       
       cJSON_AddItemToObject(array_value, "values", values_array);
       cJSON_AddItemToObject(measurements_field, "arrayValue", array_value);
       cJSON_AddItemToObject(firestore_fields, "measurements", measurements_field);
       
       // Добавляем поля к корневому объекту
       cJSON_AddItemToObject(firestore_doc, "fields", firestore_fields);
   } else {
       // Если структура уже существует, извлекаем массив measurements
       firestore_fields = cJSON_GetObjectItem(firestore_doc, "fields");
       if (!firestore_fields) {
           ESP_LOGE(TAG, "Invalid Firestore format: missing fields object");
           cJSON_Delete(firestore_doc);
           return ESP_FAIL;
       }
       
       measurements_field = cJSON_GetObjectItem(firestore_fields, "measurements");
       if (!measurements_field) {
           ESP_LOGE(TAG, "Invalid Firestore format: missing measurements field");
           cJSON_Delete(firestore_doc);
           return ESP_FAIL;
       }
       
       array_value = cJSON_GetObjectItem(measurements_field, "arrayValue");
       if (!array_value) {
           ESP_LOGE(TAG, "Invalid Firestore format: missing arrayValue");
           cJSON_Delete(firestore_doc);
           return ESP_FAIL;
       }
       
       values_array = cJSON_GetObjectItem(array_value, "values");
       if (!values_array) {
           ESP_LOGE(TAG, "Invalid Firestore format: missing values array");
           cJSON_Delete(firestore_doc);
           return ESP_FAIL;
       }
   }
   
   // Теперь добавляем новое измерение в Firestore формате
   char time_str[32];
   if (time_manager_get_formatted_time(time_str, sizeof(time_str)) != ESP_OK) {
       strcpy(time_str, "Time not available");
   }
   
   // Извлекаем только время (последние 8 символов, если доступно)
   char timestamp[9] = {0};
   if (strlen(time_str) >= 19) {
       strncpy(timestamp, time_str + 11, 8);
       timestamp[8] = '\0';
   } else {
       strncpy(timestamp, time_str, 8);
       timestamp[8] = '\0';
   }
   
   // Создаем объект для измерения
   cJSON *measurement_map_obj = cJSON_CreateObject();
   cJSON *measurement_map_value = cJSON_CreateObject();
   cJSON *measurement_map_fields = cJSON_CreateObject();
   
   // Добавляем поля температуры, влажности и времени
   
   // Температура
   cJSON *temp_field = cJSON_CreateObject();
   float rounded_temp = roundf(measurement->temperature * 100) / 100;
   char temp_str[10];
   sprintf(temp_str, "%.2f", rounded_temp);
   cJSON_AddStringToObject(temp_field, "stringValue", temp_str);
   cJSON_AddItemToObject(measurement_map_fields, "t", temp_field);
   
   // Влажность
   cJSON *hum_field = cJSON_CreateObject();
   float rounded_hum = roundf(measurement->humidity * 100) / 100;
   char hum_str[10];
   sprintf(hum_str, "%.2f", rounded_hum);
   cJSON_AddStringToObject(hum_field, "stringValue", hum_str);
   cJSON_AddItemToObject(measurement_map_fields, "h", hum_field);
   
   // Временная метка
   cJSON *timestamp_field = cJSON_CreateObject();
   cJSON_AddStringToObject(timestamp_field, "stringValue", timestamp);
   cJSON_AddItemToObject(measurement_map_fields, "ts", timestamp_field);
   
   // Добавляем в структуру
   cJSON_AddItemToObject(measurement_map_value, "fields", measurement_map_fields);
   cJSON_AddItemToObject(measurement_map_obj, "mapValue", measurement_map_value);
   cJSON_AddItemToArray(values_array, measurement_map_obj);

   // Выводим длину массива
   ESP_LOGI(TAG, "Measurements array size after adding: %d", cJSON_GetArraySize(values_array));
   
   // Сохраняем обновленную Firestore структуру
   char *firestore_json_str = cJSON_PrintUnformatted(firestore_doc);
   cJSON_Delete(firestore_doc);
   
   if (firestore_json_str == NULL) {
       return ESP_ERR_NO_MEM;
   }
   
   f = fopen(FIRESTORE_MEASUREMENTS_FILE, "w");
   if (f == NULL) {
       free(firestore_json_str);
       return ESP_FAIL;
   }
   
   fprintf(f, "%s", firestore_json_str);
   fclose(f);
   free(firestore_json_str);
   
//    // Проверка файлов
//    struct stat st_normal, st_firestore;
//    if (stat(MEASUREMENTS_FILE, &st_normal) == 0 && stat(FIRESTORE_MEASUREMENTS_FILE, &st_firestore) == 0) {
//        ESP_LOGI(TAG, "Files saved - Normal: %ld bytes, Firestore: %ld bytes", 
//                 st_normal.st_size, st_firestore.st_size);
//    } else {
//        ESP_LOGE(TAG, "File verification failed!");
//    }
   // Проверка файлов
   struct stat st_firestore;
   if (stat(FIRESTORE_MEASUREMENTS_FILE, &st_firestore) == 0) {
       ESP_LOGI(TAG, "Files saved - Firestore: %ld bytes", st_firestore.st_size);
   } else {
       ESP_LOGE(TAG, "File verification failed!");
   }

   return ESP_OK;
}

// Getting the Firestore-formatted measurements from SPIFFS
char* storage_get_firestore_measurements(void) {
   if (!check_spiffs_status()) {
       return NULL;
   }

   FILE* f = fopen(FIRESTORE_MEASUREMENTS_FILE, "r");
   if (f == NULL) {
       ESP_LOGE(TAG, "No Firestore measurements file found");
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

// Getting the measurements from SPIFFS
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

// Cleaning the measurements from SPIFFS
esp_err_t storage_clear_measurements(void) {
   return check_spiffs_status() ? unlink(MEASUREMENTS_FILE) : ESP_ERR_INVALID_STATE;
}
// Cleaning the measurements from SPIFFS
esp_err_t storage_clear_firestore_measurements(void) {
   return check_spiffs_status() ? unlink(FIRESTORE_MEASUREMENTS_FILE) : ESP_ERR_INVALID_STATE;
}

// Storaging the logs in SPIFFS
esp_err_t storage_append_log(const char* log_message) {
    #if DISCORD_LOGGING
    if (!check_spiffs_status()) {
        return ESP_ERR_INVALID_STATE;
    }
    
    const char* LOG_FILE = "/spiffs/debug_log.txt";
    
    // Opening a file in append mode
    FILE* f = fopen(LOG_FILE, "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open log file");
        return ESP_FAIL;
    }
    
    // Adding a timestamp and a message
    uint32_t boot_count = storage_get_boot_count();
    fprintf(f, "[Boot:%lu][%lld] %s\n", boot_count, (long long)(esp_timer_get_time() / 1000000), log_message);
    fclose(f);
    #endif
    
    return ESP_OK;
}

// Getting the logs from SPIFFS
char* storage_get_logs(void) {
    #if DISCORD_LOGGING
    static const char* TAG = "storage";
    
    if (!check_spiffs_status()) {
        ESP_LOGE(TAG, "SPIFFS not mounted, cannot read logs");
        return NULL;
    }

    const char* LOG_FILE = "/spiffs/debug_log.txt";
    
    // Check if file exists first
    struct stat st;
    if (stat(LOG_FILE, &st) != 0) {
        ESP_LOGE(TAG, "Log file not found");
        return NULL;
    }
    
    // Check file size
    if (st.st_size <= 0) {
        ESP_LOGE(TAG, "Log file is empty (size: %ld)", st.st_size);
        return NULL;
    }
    
    ESP_LOGI(TAG, "Log file size: %ld bytes", st.st_size);
    
    FILE* f = fopen(LOG_FILE, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open log file");
        return NULL;
    }

    // Get file size using fseek/ftell
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (fsize <= 0) {
        ESP_LOGE(TAG, "Invalid file size: %ld", fsize);
        fclose(f);
        return NULL;
    }
    
    ESP_LOGI(TAG, "Allocating %ld bytes for logs", fsize + 1);
    
    // Allocate memory with safety margin
    char *log_str = malloc(fsize + 16);
    if (log_str == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for logs");
        fclose(f);
        return NULL;
    }

    // Read file in chunks to handle large files
    size_t bytes_read = 0;
    size_t total_read = 0;
    const size_t chunk_size = 1024;
    
    while (total_read < (size_t)fsize) {
        bytes_read = fread(log_str + total_read, 1, 
                          ((size_t)fsize - total_read > chunk_size) ? 
                           chunk_size : ((size_t)fsize - total_read), 
                          f);
        
        if (bytes_read == 0) {
            if (feof(f)) {
                ESP_LOGW(TAG, "End of file reached before expected size");
                break;
            }
            if (ferror(f)) {
                ESP_LOGE(TAG, "Error reading log file");
                free(log_str);
                fclose(f);
                return NULL;
            }
        }
        
        total_read += bytes_read;
    }
    
    fclose(f);
    
    // Ensure null termination
    log_str[total_read] = '\0';
    
    ESP_LOGI(TAG, "Successfully read %zu bytes from log file", total_read);
    
    return log_str;
    #else
    return NULL;
    #endif
}


// -------------------- Error flag operations -------------------- //

// Setting the error flag
esp_err_t storage_set_error_flag(void) {
    esp_err_t ret = nvs_set_u8(my_nvs_handle, "error_flag", 1);
    return (ret != ESP_OK) ? ret : nvs_commit(my_nvs_handle);
}

// Getting the error flag
bool storage_get_error_flag(void) {
    uint8_t flag = 0;
    esp_err_t ret = nvs_get_u8(my_nvs_handle, "error_flag", &flag);
    return (ret == ESP_OK && flag == 1);
}

// Clearing the error flag
esp_err_t storage_clear_error_flag(void) {
    esp_err_t ret = nvs_set_u8(my_nvs_handle, "error_flag", 0);
    return (ret != ESP_OK) ? ret : nvs_commit(my_nvs_handle);
}

// -------------------- System state operations -------------------- //


esp_err_t storage_set_system_state(system_state_t state) {
    esp_err_t ret = nvs_set_u8(my_nvs_handle, "system_state", (uint8_t)state);
    if (ret != ESP_OK) return ret;
    return nvs_commit(my_nvs_handle);
}

system_state_t storage_get_system_state(void) {
    uint8_t state = STATE_NORMAL;
    esp_err_t ret = nvs_get_u8(my_nvs_handle, "system_state", &state);
    return (ret == ESP_OK) ? (system_state_t)state : STATE_NORMAL;
}

bool storage_is_first_boot(void) {
    uint8_t first_boot = 0;
    esp_err_t ret = nvs_get_u8(my_nvs_handle, FIRST_BOOT_KEY, &first_boot);
    return (ret == ESP_ERR_NVS_NOT_FOUND || first_boot == 0);
}

esp_err_t storage_mark_first_boot_completed(void) {
    esp_err_t ret = nvs_set_u8(my_nvs_handle, FIRST_BOOT_KEY, 1);
    return (ret != ESP_OK) ? ret : nvs_commit(my_nvs_handle);
}

