idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES gsm_module json_helper sensors power_management cloud_client storage nvs_flash esp_hw_support esp_pm)