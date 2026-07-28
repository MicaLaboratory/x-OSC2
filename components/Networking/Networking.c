/**
 * @file Networking.c
 * @author Ben Marples
 */

//------------------------------------------------------------------------------
// Includes

#include <stdio.h>
#include <string.h>
#include "Networking.h"
#include "NVS_Helper_Funcs.h"
#include "esp_wifi.h"

//------------------------------------------------------------------------------
// Variables

//------------------------------------------------------------------------------
// Function declarations

esp_err_t wifi_configure_softap(const NVS_Global *ctx, wifi_config_t *cfg);

//------------------------------------------------------------------------------
// Functions

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
