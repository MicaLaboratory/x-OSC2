idf_component_register(SRCS "softap_example_main.c"
                    PRIV_REQUIRES esp_wifi nvs_flash nvs_config esp_http_server OSC99
                    INCLUDE_DIRS ".")