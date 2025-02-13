# components/gsm_module/CMakeLists.txt
idf_component_register(
    SRCS "gsm_module.c"
    INCLUDE_DIRS "include"
    REQUIRES driver)