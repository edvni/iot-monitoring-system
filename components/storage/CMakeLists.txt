idf_component_register(
    SRCS "storage.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash spiffs json json_helper sensors system_states battery_monitor
)