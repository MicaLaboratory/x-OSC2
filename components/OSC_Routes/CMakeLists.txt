idf_component_register(SRCS "OSC_Routes.c"
                    REQUIRES OSC99 esp_driver_gpio NVS_Helper_Funcs
                    INCLUDE_DIRS "include")
