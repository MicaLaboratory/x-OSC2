idf_component_register(SRCS "main.c"
                    REQUIRES NVS_Helper_Funcs esp_driver_gpio Networking esp_http_server cjson OSC99 esp_adc OSC_Routes
                    INCLUDE_DIRS ".")
