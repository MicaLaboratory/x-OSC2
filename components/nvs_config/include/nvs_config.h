
#ifndef NVS_CONFIG
#define NVS_CONFIG

#include <stdio.h>



#include <stdint.h>
void nvs_save_int(const char *namespace_name, const char *key, int32_t value);
int32_t nvs_load_int(const char *namespace_name, const char *key, int32_t default_value);
void print_all_nvs_entries(const char *namespace);

#endif