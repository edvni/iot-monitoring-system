# components/json_helper/CMakeLists.txt
idf_component_register(
    SRCS "json_helper.c"
    INCLUDE_DIRS "include"
    REQUIRES json sensors
    )