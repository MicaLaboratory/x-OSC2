/**
 * @file Networking.c
 * @author Ben Marples
 */

//------------------------------------------------------------------------------
// Includes

#include <stdio.h>
#include <string.h>
#include "Networking.h"
#include "settings.h"
#include "NVS_Helper_Funcs.h"
#include "esp_wifi.h"
#include "lwip/sockets.h"

//------------------------------------------------------------------------------
// Definitions

typedef struct
{
    int handle;
} Socket;

//------------------------------------------------------------------------------
// Variables

static Socket sock;
static WirelessCallbacks callbacks;
static Osc_Settings_t nvs_osc;

//------------------------------------------------------------------------------
// Function declarations

void networking_init(const Osc_Settings_t *cfg,const WirelessCallbacks *callbacks_);
esp_err_t wifi_configure_softap(const NVS_Global *ctx, wifi_config_t *cfg);
esp_err_t wifi_configure_station(const NVS_Global *ctx, wifi_config_t *cfg);
static void nvs_osc_configure(const Osc_Settings_t *cfg);
static socket_err socket_init();
static void receive_task(void *pvParameters);
int udp_send_osc(const char *contents, const size_t size);

//------------------------------------------------------------------------------
// Functions
void networking_init(const Osc_Settings_t *cfg,const WirelessCallbacks *callbacks_)
{
    nvs_osc_configure(cfg);
    callbacks = *callbacks_;
    socket_init();
    xTaskCreate(receive_task, "receive_task", 12288, (void *)AF_INET, 5, NULL);
}

esp_err_t wifi_configure_softap(const NVS_Global *ctx, wifi_config_t *cfg)
{
    // Clear struct (optional if caller zeroes it)
    memset(cfg, 0, sizeof(*cfg));

    // Copy SSID safely
    strlcpy((char *)cfg->ap.ssid, ctx->net_settings.ap_ssid, sizeof(cfg->ap.ssid));

    // Copy password safely
    strlcpy((char *)cfg->ap.password, ctx->net_settings.ap_password, sizeof(cfg->ap.password));

    // Channel, max clients, PMF
    cfg->ap.channel = CONFIG_ESP_WIFI_AP_CHANNEL;
    cfg->ap.max_connection = CONFIG_ESP_MAX_STA_CONN_AP;
    cfg->ap.pmf_cfg.required = false;

    // Auth mode based on password presence
    cfg->ap.authmode = (cfg->ap.password[0] == '\0') ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;

    return esp_wifi_set_config(WIFI_IF_AP, cfg);
}

esp_err_t wifi_configure_station(const NVS_Global *ctx, wifi_config_t *cfg)
{
    // Clear the struct (caller may also do this)
    memset(cfg, 0, sizeof(*cfg));

    // Safe copy SSID
    strlcpy((char *)cfg->sta.ssid, ctx->net_settings.sta_ssid, sizeof(cfg->sta.ssid));

    // Safe copy password
    strlcpy((char *)cfg->sta.password, ctx->net_settings.sta_password, sizeof(cfg->sta.password));

    // Retry count
    cfg->sta.failure_retry_cnt = CONFIG_ESP_MAXIMUM_STA_RETRY;

    // WPA2/WPA3 threshold
    cfg->sta.threshold.authmode = WIFI_AUTH_OPEN;

    // WPA3 SAE PWE mode
    cfg->sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;

    return esp_wifi_set_config(WIFI_IF_STA, cfg);
}

static void nvs_osc_configure(const Osc_Settings_t *cfg)
{
    nvs_osc = *cfg;
}

static socket_err socket_init()
{
    sock.handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock.handle == -1)
    {
        return SOCK_errno;
    }

    const struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_port = htons(nvs_osc.local_port),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    int err = bind(sock.handle, (struct sockaddr *)&address, sizeof(address));
    if (err == -1)
    {
        return SOCK_BIND;
    }

    return SOCK_OK;
}

static void receive_task(void *pvParameters)
{

    while (true)
    {
        if (sock.handle == -1)
        {
            vTaskDelay(1); // not open yet / closed
            continue;
        }

        static uint8_t data[1472];
        const ssize_t number_of_bytes = recvfrom(sock.handle, data, sizeof(data), 0, NULL, NULL);

        if (number_of_bytes <= 0)
        {
            vTaskDelay(1); // error / socket closed → back off
            continue;
        }

        if (callbacks.received != NULL)
        {
            callbacks.received(data, (size_t)number_of_bytes);
        }
    }
}

int udp_send_osc(const char *contents, const size_t size)
{
    if (sock.handle < 0)
    {
        // TODO - 
        // ESP_LOGE("UDP_Send", "Socket not initialized");
        return -1;
    }

    const struct sockaddr_in dest_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(nvs_osc.remote_port),
        .sin_addr.s_addr = inet_addr(nvs_osc.remote_ip),
    };

    int err = sendto(
        sock.handle,
        contents,
        size,
        0,
        (struct sockaddr *)&dest_addr, sizeof(dest_addr));

    return err;
}