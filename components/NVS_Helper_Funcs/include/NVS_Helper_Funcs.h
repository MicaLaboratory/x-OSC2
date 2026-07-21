
#ifndef NVS_HELPER_H
#define NVS_HELPER_H


#include <stdint.h>
#include "esp_err.h"
#include "nvs.h"

// typedef enum {
//     NVS_TYPE_I32,
//     NVS_TYPE_U32,
//     NVS_TYPE_I8,
//     NVS_TYPE_U8,
//     NVS_TYPE_STR,
//     NVS_TYPE_BLOB
// } nvs_value_type_t;

typedef struct {
    const void *data;
    size_t size;
} nvs_blob_t;

typedef enum
{
    FIELD_U32,
    FIELD_U16,
    FIELD_BOOL,
    FIELD_ENUM,
    FIELD_STR
} FieldType;


void print_all_nvs_entries(const char *namespace);

esp_err_t nvs_save_value(const char *namespace_name, const char *key, nvs_type_t type, const void *value);
esp_err_t nvs_load_value(const char *namespace_name, const char *key, FieldType type, const void *default_value, void *out_value);


#endif