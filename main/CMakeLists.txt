idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES gsm_module json_helper sensors storage cloud_client storage nvs_flash esp_hw_support esp_pm)