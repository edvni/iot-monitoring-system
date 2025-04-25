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
#include <dirent.h> 

#define MEASUREMENTS_FILE   "/spiffs/measurements.json"
#define FIRESTORE_MEASUREMENTS_FILE "/spiffs/firestore_measurements.json"
#define FIRST_BOOT_KEY "first_boot"

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

// function for generating a file name based on the MAC address
static void generate_sensor_filename(char *filename, size_t max_length, const char *mac_address) {
    char formatted_mac[18];
    
    // Replace colons with underscores to create a valid file name
    strncpy(formatted_mac, mac_address, sizeof(formatted_mac));
    for (int i = 0; i < strlen(formatted_mac); i++) {
        if (formatted_mac[i] == ':') {
            formatted_mac[i] = '_';
        }
    }
    
    // Forming the file path
    snprintf(filename, max_length, "/spiffs/sensor_%s.json", formatted_mac);
}


// Saving the measurement to SPIFFS
esp_err_t storage_save_measurement(ruuvi_measurement_t *measurement) {
   if (!check_spiffs_status()) {
       return ESP_ERR_INVALID_STATE;
   }

   // Generating a file name based on the MAC address
   char sensor_filename[64];
   generate_sensor_filename(sensor_filename, sizeof(sensor_filename), measurement->mac_address);
   
   ESP_LOGI(TAG, "Saving measurement from %s to file: %s", measurement->mac_address, sensor_filename);

   // Loading the existing Firestore structure, if it exists
   cJSON *firestore_doc = NULL;
   FILE* f = fopen(sensor_filename, "r");
   if (f != NULL) {
       // If the file exists, load the structure
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
   
   // Creating or updating the Firestore document
   firestore_doc = json_helper_create_or_update_firestore_document(firestore_doc, measurement->mac_address);
   if (firestore_doc == NULL) {
       ESP_LOGE(TAG, "Failed to create Firestore document");
       return ESP_FAIL;
   }
   
   // Adding the measurement to the document
   esp_err_t result = json_helper_add_measurement_to_firestore(firestore_doc, measurement);
   if (result != ESP_OK) {
       ESP_LOGE(TAG, "Failed to add measurement to Firestore document");
       cJSON_Delete(firestore_doc);
       return result;
   }
   
   // Getting the size of the measurements array for logging
   cJSON *fields = cJSON_GetObjectItem(firestore_doc, "fields");
   cJSON *measurements = cJSON_GetObjectItem(fields, "measurements");
   cJSON *array_value = cJSON_GetObjectItem(measurements, "arrayValue");
   cJSON *values = cJSON_GetObjectItem(array_value, "values");
   ESP_LOGI(TAG, "Measurements array size after adding: %d", cJSON_GetArraySize(values));
   
   // Saving the updated Firestore structure
   char *firestore_json_str = cJSON_PrintUnformatted(firestore_doc);
   cJSON_Delete(firestore_doc);
   
   if (firestore_json_str == NULL) {
       return ESP_ERR_NO_MEM;
   }
   
   f = fopen(sensor_filename, "w");
   if (f == NULL) {
       free(firestore_json_str);
       return ESP_FAIL;
   }
   
   fprintf(f, "%s", firestore_json_str);
   fclose(f);
   free(firestore_json_str);
   
   // Checking the file
   struct stat st;
   if (stat(sensor_filename, &st) == 0) {
       ESP_LOGI(TAG, "File saved for sensor %s: %ld bytes", measurement->mac_address, st.st_size);
   } else {
       ESP_LOGE(TAG, "File verification failed!");
   }

   return ESP_OK;
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

// Function for getting a list of sensor files
esp_err_t storage_get_sensor_files(char ***file_list, int *file_count) {
    if (!check_spiffs_status()) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Temporarily set the maximum number of files
    const int MAX_FILES = 10;
    char **files = malloc(MAX_FILES * sizeof(char*));
    if (files == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize the file counter
    *file_count = 0;
    
    // Opening the SPIFFS directory
    DIR *dir = opendir("/spiffs");
    if (dir == NULL) {
        ESP_LOGE(TAG, "Failed to open SPIFFS directory");
        free(files);
        return ESP_FAIL;
    }
    
    // Reading the directory content
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && *file_count < MAX_FILES) {
        // Checking if the file starts with "sensor_"
        if (strncmp(entry->d_name, "sensor_", 7) == 0) {
            // Allocating memory for the file name and copying it
            char *filename = malloc(strlen(entry->d_name) + 9); // +9 для "/spiffs/" и '\0'
            if (filename) {
                sprintf(filename, "/spiffs/%s", entry->d_name);
                files[*file_count] = filename;
                (*file_count)++;
                ESP_LOGI(TAG, "Found sensor file: %s", filename);
            }
        }
    }
    
    closedir(dir);
    
    // If no files are found
    if (*file_count == 0) {
        free(files);
        *file_list = NULL;
        return ESP_OK;
    }
    
    // Setting the pointer to the list of files
    *file_list = files;
    
    return ESP_OK;
}

// Clearing the list of sensor files
void storage_free_sensor_files(char **file_list, int file_count) {
    if (file_list) {
        for (int i = 0; i < file_count; i++) {
            if (file_list[i]) {
                free(file_list[i]);
            }
        }
        free(file_list);
    }
}

// Synchronizing the file system
esp_err_t storage_sync(void) {
    ESP_LOGI(TAG, "Synchronizing file system");
    FILE *f = fopen("/spiffs/.sync", "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for sync");
        return ESP_FAIL;
    }
    fprintf(f, "sync");
    fsync(fileno(f));
    fclose(f);
    unlink("/spiffs/.sync");
    return ESP_OK;
}

