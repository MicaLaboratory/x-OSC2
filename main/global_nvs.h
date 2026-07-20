#ifndef GLOBAL_NVS_IMPL_H
#define GLOBAL_NVS_IMPL_H

#include <stdbool.h>
#include <cstdint>
#include <cstddef>


#define PINCOUNT 28

typedef enum
{
    AP = 1,
    STA,
    AP_MODE_END
} AP_Mode;

typedef enum {
    GPIO_OFF,
    GPIO_ANALOGUE,
    GPIO_DIGITAL
}GPIO_State;

typedef enum {
    GPIO_OUTPUT,
    GPIO_INPUT,
}GPIO_IO;

typedef struct {
    AP_Mode network_mode;
    const char *ap_ssid;
    const char *ap_password;
    const char *sta_ssid;
    const char *sta_password;
}Network_Setttings_t;

typedef struct {
    const char * remote_ip;
    uint16_t remote_port;
    const char * local_ip;
    uint16_t local_port;
    bool bundle;
    bool address_prefix;
}Osc_Settings_t;

typedef struct {
    uint32_t gpio_rate;
    GPIO_State pin_mode[PINCOUNT];
    GPIO_IO pin_io[PINCOUNT];
}GPIO_Settings_t;

typedef struct {
    Network_Setttings_t net_settings;
    Osc_Settings_t osc_settings;
    GPIO_Settings_t gpio_settings;
}NVS_Global;

typedef enum {
    FIELD_U32,
    FIELD_U16,
    FIELD_BOOL,
    FIELD_ENUM,
    FIELD_STR
} FieldType;

typedef struct {
    const char *name;     // full path string
    // size_t offset;        // offsetof()
    FieldType type;       // type enum
} FieldDesc;

static const FieldDesc g_fields[] = {

    /* -------------------------------
       Network Settings
       ------------------------------- */
    { "net_settings.network_mode",   FIELD_ENUM },
    { "net_settings.ap_ssid",        FIELD_STR  },
    { "net_settings.ap_password",    FIELD_STR  },
    { "net_settings.sta_ssid",       FIELD_STR  },
    { "net_settings.sta_password",   FIELD_STR  },

    /* -------------------------------
       OSC Settings
       ------------------------------- */
    { "osc_settings.remote_ip",      FIELD_STR  },
    { "osc_settings.remote_port",    FIELD_U16  },
    { "osc_settings.local_ip",       FIELD_STR  },
    { "osc_settings.local_port",     FIELD_U16  },
    { "osc_settings.bundle",         FIELD_BOOL },
    { "osc_settings.address_prefix", FIELD_BOOL },

    /* -------------------------------
       GPIO Settings
       ------------------------------- */
    { "gpio_settings.gpio_rate",     FIELD_U32  },
};


#endif 