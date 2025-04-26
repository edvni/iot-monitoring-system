idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES esp_timer 
    nvs_flash 
    power_management 
    sensors 
    storage 
    gsm_modem 
    discord_api 
    time_manager 
    battery_monitor
    reporter
    firebase_api
    system_states
)