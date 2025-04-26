idf_component_register(
    SRCS "reporter.c"
    INCLUDE_DIRS "include"
    REQUIRES battery_monitor discord_api storage system_state
) 