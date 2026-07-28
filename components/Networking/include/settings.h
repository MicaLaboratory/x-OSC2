#ifndef SETTINGS_H
#define SETTINGS_H

typedef struct
{

} AP_cfg;

typedef struct
{

} Station;

typedef struct
{

} Network_Settings_t;

typedef struct
{
    // void (*client_connected)(const char *const ssid, const WirelessClientChannel channel);
    // void (*client_disconnected)(const char *const ssid);
    // void (*got_ip)(const uint32_t ip_address);
    // void (*send_error)(const WirelessResult result);
    void (*received)(const void *const data, const size_t number_of_bytes);
    // void (*unhandled_wifi_event)(const char *const event);
    // void (*unhandled_ip_event)(const char *const event);
} WirelessCallbacks;

#endif