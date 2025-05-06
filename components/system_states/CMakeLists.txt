idf_component_register(
    SRCS "system_states.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash freertos esp_system storage gsm_modem config_manager
)