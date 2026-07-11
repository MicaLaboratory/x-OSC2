
#ifndef NVS_HELPER_H
#define NVS_HELPER_H


#include <stdint.h>
#include "esp_err.h"


void print_all_nvs_entries(const char *namespace);

esp_err_t nvs_save_str(const char *namespace_name, const char *key, const char *value);
esp_err_t nvs_save_int(const char *namespace_name, const char *key, int32_t value);

char *nvs_load_str(const char *namespace_name, const char *key, const char *default_value);
int32_t nvs_load_int(const char *namespace_name, const char *key, int32_t default_value);


#endif