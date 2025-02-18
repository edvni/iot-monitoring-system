idf_component_register(
    SRCS "sensors.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_common esp_timer log bt time_manager driver)