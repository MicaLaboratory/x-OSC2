
#ifndef NVS_HELPER_H
#define NVS_HELPER_H


#include <stdint.h>
#include "esp_err.h"

typedef enum {
    NVS_TYPE_I32,
    NVS_TYPE_U32,
    NVS_TYPE_I8,
    NVS_TYPE_U8,
    NVS_TYPE_STR,
    NVS_TYPE_BLOB
} nvs_value_type_t;

typedef struct {
    const void *data;
    size_t size;
} nvs_blob_t;


void print_all_nvs_entries(const char *namespace);

esp_err_t nvs_save_value(const char *namespace_name, const char *key, nvs_value_type_t type, const void *value);

char *nvs_load_str(const char *namespace_name, const char *key, const char *default_value);
int32_t nvs_load_int(const char *namespace_name, const char *key, int32_t default_value);


#endif