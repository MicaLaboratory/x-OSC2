#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h" 
#include "main.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "soc/gpio_num.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif_net_stack.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#if IP_NAPT
#include "lwip/lwip_napt.h"
#endif
#include "lwip/err.h"
#include "lwip/sys.h"

#include "esp_http_server.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"


#define PLACEHOLDER 0
#define PINCOUNT 26
#define MAX_ATTEMPS 10


/* STA Configuration */
#define EXAMPLE_ESP_WIFI_STA_SSID           CONFIG_ESP_WIFI_REMOTE_AP_SSID
#define EXAMPLE_ESP_WIFI_STA_PASSWD         CONFIG_ESP_WIFI_REMOTE_AP_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY           CONFIG_ESP_MAXIMUM_STA_RETRY

#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_WAPI_PSK
#endif

/* AP Configuration */
#define EXAMPLE_ESP_WIFI_AP_SSID            CONFIG_ESP_WIFI_AP_SSID
#define EXAMPLE_ESP_WIFI_AP_PASSWD          CONFIG_ESP_WIFI_AP_PASSWORD
#define EXAMPLE_ESP_WIFI_CHANNEL            CONFIG_ESP_WIFI_AP_CHANNEL
#define EXAMPLE_MAX_STA_CONN                CONFIG_ESP_MAX_STA_CONN_AP


/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

/*DHCP server option*/
#define DHCPS_OFFER_DNS             0x02


static const char *TAG_AP = "WiFi SoftAP";
static const char *TAG_STA = "WiFi Sta";

static int s_retry_num = 0;


// NVS 
void nvs_save_int(const char *namespace_name, const char *key, int32_t value)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Error opening NVS: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_set_i32(handle, key, value);
    if (err == ESP_OK) {
        nvs_commit(handle);
        ESP_LOGI("NVS", "Saved %s = %ld", key, value);
    } else {
        ESP_LOGE("NVS", "Failed to save %s: %s", key, esp_err_to_name(err));
    }

    nvs_close(handle);
}

void nvs_save_str(const char *namespace_name, const char *key, const char *value)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Error opening NVS: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_set_str(handle, key, value);
    if (err == ESP_OK) {
        nvs_commit(handle);
        ESP_LOGI("NVS", "Saved %s = \"%s\"", key, value);
    } else {
        ESP_LOGE("NVS", "Failed to save %s: %s", key, esp_err_to_name(err));
    }

    nvs_close(handle);
}

int32_t nvs_load_int(const char *namespace_name, const char *key, int32_t default_value)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW("NVS", "Namespace not found, using default");
        return default_value;
    }

    int32_t value = default_value;
    err = nvs_get_i32(handle, key, &value);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW("NVS", "Key %s not found, using default", key);
    } else if (err != ESP_OK) {
        ESP_LOGE("NVS", "Error reading %s: %s", key, esp_err_to_name(err));
    }

    nvs_close(handle);
    return value;
}

char *nvs_load_str(const char *namespace_name, const char *key, const char *default_value)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW("NVS", "Namespace not found, using default");
        return strdup(default_value);
    }

    size_t len = 0;
    err = nvs_get_str(handle, key, NULL, &len);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW("NVS", "Key %s not found, using default", key);
        nvs_close(handle);
        return strdup(default_value);
    }

    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Error reading %s: %s", key, esp_err_to_name(err));
        nvs_close(handle);
        return strdup(default_value);
    }

    char *value = malloc(len);
    if (!value) {
        ESP_LOGE("NVS", "Out of memory");
        nvs_close(handle);
        return strdup(default_value);
    }

    err = nvs_get_str(handle, key, value, &len);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Error reading %s: %s", key, esp_err_to_name(err));
        free(value);
        nvs_close(handle);
        return strdup(default_value);
    }

    nvs_close(handle);
    return value;
}

void print_all_nvs_entries(const char *namespace) {
    
    nvs_iterator_t it = NULL;
    esp_err_t err = nvs_entry_find("nvs", namespace, NVS_TYPE_ANY, &it);

    while (err == ESP_OK && it != NULL) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        printf("Key: %s, Type: %d\n", info.key, info.type);

        // If you want to read the value:
        nvs_handle_t handle;
        if (nvs_open(namespace, NVS_READONLY, &handle) == ESP_OK) {
            if (info.type == NVS_TYPE_STR) {
                size_t len;
                nvs_get_str(handle, info.key, NULL, &len);
                char *value = malloc(len);
                if (value) {
                    nvs_get_str(handle, info.key, value, &len);
                    printf("  Value: %s\n", value);
                    free(value);
                }
            } else if (info.type == NVS_TYPE_I32) {
                int32_t val;
                nvs_get_i32(handle, info.key, &val);
                printf("  Value: %" PRId32 "\n", val);
            }
            nvs_close(handle);
        }

        err = nvs_entry_next(&it);
    }
    nvs_release_iterator(it);
}

void init_default_Config(){

    // Set Default Network Settings
    nvs_save_int("Config","Network",AP);
    // AP
    nvs_save_str("AP","SSID","x-osc-2");
    nvs_save_str("AP","Passphrase","mypassword");
    // STA
    nvs_save_int("STA","SSID",PLACEHOLDER);
    nvs_save_int("STA","Passphrase",PLACEHOLDER);

    // Sets OSC message settings
    nvs_save_int("OSC","Remote_IP",PLACEHOLDER);
    nvs_save_int("OSC","Remote_Port",PLACEHOLDER);
    nvs_save_int("OSC","Local_IP",PLACEHOLDER);
    nvs_save_int("OSC","Local_Port",PLACEHOLDER);

    nvs_save_int("OSC","Bundles",PLACEHOLDER);
    nvs_save_int("OSC","address_Prefix",PLACEHOLDER);

    // Sets Pin GPIO 
    // All pins to begin with are set to OFF 
    for (int i = 1; i < PINCOUNT; i++){
        char buf[64];
        sprintf(buf,"Pin-%d-",i); 
        char IO[32] = "IO";
        strcat(buf, IO);
        nvs_save_int("Pins",buf,0);
        sprintf(buf,"Pin-%d-",i); 
        char PType[32] = "PType";
        strcat(buf,PType);
        nvs_save_int("Pins",buf,0);
    }
    



}


/* FreeRTOS event group to signal when we are connected/disconnected */
static EventGroupHandle_t s_wifi_event_group;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *) event_data;
        ESP_LOGI(TAG_AP, "Station "MACSTR" joined, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *) event_data;
        ESP_LOGI(TAG_AP, "Station "MACSTR" left, AID=%d, reason:%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG_STA, "Station started");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG_STA, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_ASSIGNED_IP_TO_CLIENT) {
        const ip_event_assigned_ip_to_client_t *e = (const ip_event_assigned_ip_to_client_t *)event_data;
        ESP_LOGI(TAG_AP, "Assigned IP to client: " IPSTR ", MAC=" MACSTR ", hostname='%s'",
                 IP2STR(&e->ip), MAC2STR(e->mac), e->hostname);
    }
}

/* Initialize soft AP */
void wifi_init_softap(void)
{
    // esp_netif_t *esp_netif_ap = esp_netif_create_default_wifi_ap();

    wifi_config_t wifi_ap_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_AP_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_AP_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_AP_PASSWD,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .required = false,
            },
        },
    };

    if (strlen(EXAMPLE_ESP_WIFI_AP_PASSWD) == 0) {
        wifi_ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));

    ESP_LOGI(TAG_AP, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             EXAMPLE_ESP_WIFI_AP_SSID, EXAMPLE_ESP_WIFI_AP_PASSWD, EXAMPLE_ESP_WIFI_CHANNEL);

    // return esp_netif_ap;
}

/* Initialize wifi station */
void wifi_init_sta(void)
{
    // esp_netif_t *esp_netif_sta = esp_netif_create_default_wifi_sta();

    wifi_config_t wifi_sta_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_STA_SSID,
            .password = EXAMPLE_ESP_WIFI_STA_PASSWD,
            .scan_method = WIFI_ALL_CHANNEL_SCAN,
            .failure_retry_cnt = EXAMPLE_ESP_MAXIMUM_RETRY,
            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (password len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
            * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config) );

    ESP_LOGI(TAG_STA, "wifi_init_sta finished.");

    // return esp_netif_sta;
}

void softap_set_dns_addr(esp_netif_t *esp_netif_ap,esp_netif_t *esp_netif_sta)
{
    esp_netif_dns_info_t dns;
    esp_netif_get_dns_info(esp_netif_sta,ESP_NETIF_DNS_MAIN,&dns);
    uint8_t dhcps_offer_option = DHCPS_OFFER_DNS;
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_stop(esp_netif_ap));
    ESP_ERROR_CHECK(esp_netif_dhcps_option(esp_netif_ap, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &dhcps_offer_option, sizeof(dhcps_offer_option)));
    ESP_ERROR_CHECK(esp_netif_set_dns_info(esp_netif_ap, ESP_NETIF_DNS_MAIN, &dns));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_start(esp_netif_ap));
}

static const char *TAG = "HTTP_SERVER";

void handler(httpd_req_t *req)
{
    int sockfd = httpd_req_to_sockfd(req);

    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);

    getpeername(sockfd, (struct sockaddr *)&addr, &addr_len);

    if (addr.ss_family == AF_INET) {
        struct sockaddr_in *addr_in = (struct sockaddr_in *)&addr;
        char ip[16];
        inet_ntop(AF_INET, &addr_in->sin_addr, ip, sizeof(ip));

        ESP_LOGI("HTTP", "Client IP: %s", ip);
    }
}


/* An HTTP GET handler */
static esp_err_t base_handler(httpd_req_t *req)
{
    print_all_nvs_entries("Global-Config");
    const char* resp_str = "<head><style>table {display: flex;justify-content: center; /* Centers horizontally */align-items: center; /* Centers vertically */height: 100vh; /* Full viewport height */}   tr,th,td {    border:1px solid black;}</style></head><body><form method=\"get\" action=\"/save\"> <table><tr><td>Pin - 1</td><td><select name=\"pin1\"> <option value=\"0\">OFF</option><option value=\"1\">Analog</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 2</td><td><select name=\"pin2\"> <option value=\"0\">OFF</option><option value=\"1\">Analog</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 3</td><td><select name=\"pin3\"> <option value=\"0\">OFF</option><option value=\"1\">Analog</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 4</td><td><select name=\"pin4\"> <option value=\"0\">OFF</option><option value=\"1\">Analog</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 5</td><td><select name=\"pin5\"> <option value=\"0\">OFF</option><option value=\"1\">Analog</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 6</td><td><select name=\"pin6\"> <option value=\"0\">OFF</option><option value=\"1\">Analog</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 7</td><td><select name=\"pin7\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 8</td><td><select name=\"pin8\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 9</td><td><select name=\"pin9\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 10</td><td><select name=\"pin10\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 11</td><td><select name=\"pin11\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 12</td><td><select name=\"pin12\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 13</td><td><select name=\"pin13\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 14</td><td><select name=\"pin14\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 15</td><td><select name=\"pin15\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 16</td><td><select name=\"pin16\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 17</td><td><select name=\"pin17\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 18</td><td><select name=\"pin18\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 19</td><td><select name=\"pin19\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 20</td><td><select name=\"pin20\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 21</td><td><select name=\"pin21\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 22</td><td><select name=\"pin22\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 23</td><td><select name=\"pin23\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 24</td><td><select name=\"pin24\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 25</td><td><select name=\"pin25\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 26</td><td><select name=\"pin26\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 27</td><td><select name=\"pin27\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 28</td><td><select name=\"pin28\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr></table><button type=\"submit\">Save </button></form></body>    ";
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;

}

static esp_err_t setAP(httpd_req_t *req)
{
    nvs_save_int("Config","Network",AP);
    const char* resp_str = "<head><style>table {display: flex;justify-content: center; /* Centers horizontally */align-items: center; /* Centers vertically */height: 100vh; /* Full viewport height */}   tr,th,td {    border:1px solid black;}</style></head><body><form method=\"get\" action=\"/save\"> <table><tr><td>Pin - 1</td><td><select name=\"pin1\"> <option value=\"0\">OFF</option><option value=\"1\">Analog</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 2</td><td><select name=\"pin2\"> <option value=\"0\">OFF</option><option value=\"1\">Analog</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 3</td><td><select name=\"pin3\"> <option value=\"0\">OFF</option><option value=\"1\">Analog</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 4</td><td><select name=\"pin4\"> <option value=\"0\">OFF</option><option value=\"1\">Analog</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 5</td><td><select name=\"pin5\"> <option value=\"0\">OFF</option><option value=\"1\">Analog</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 6</td><td><select name=\"pin6\"> <option value=\"0\">OFF</option><option value=\"1\">Analog</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 7</td><td><select name=\"pin7\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 8</td><td><select name=\"pin8\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 9</td><td><select name=\"pin9\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 10</td><td><select name=\"pin10\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 11</td><td><select name=\"pin11\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 12</td><td><select name=\"pin12\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 13</td><td><select name=\"pin13\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 14</td><td><select name=\"pin14\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 15</td><td><select name=\"pin15\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 16</td><td><select name=\"pin16\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 17</td><td><select name=\"pin17\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 18</td><td><select name=\"pin18\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 19</td><td><select name=\"pin19\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 20</td><td><select name=\"pin20\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 21</td><td><select name=\"pin21\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 22</td><td><select name=\"pin22\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 23</td><td><select name=\"pin23\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 24</td><td><select name=\"pin24\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 25</td><td><select name=\"pin25\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 26</td><td><select name=\"pin26\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 27</td><td><select name=\"pin27\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr><tr><td>Pin - 28</td><td><select name=\"pin28\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td></tr></table><button type=\"submit\">Save </button></form></body>    ";
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    esp_restart();
    return ESP_OK;

}

static esp_err_t setSTA(httpd_req_t *req)
{   
    nvs_save_int("Config","Network",STA);
    const char* resp_str = "RECIEVED STA";
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    esp_restart();
    return ESP_OK;

}

static const httpd_uri_t base_uri= {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = base_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t setAP_uri= {
    .uri       = "/setAP",
    .method    = HTTP_GET,
    .handler   = setAP,
    .user_ctx  = NULL
};

static const httpd_uri_t setSTA_uri= {
    .uri       = "/setSTA",
    .method    = HTTP_GET,
    .handler   = setSTA,
    .user_ctx  = NULL
};

// Defines the Full Http server
httpd_handle_t start_webserver(){

  httpd_handle_t server = NULL;

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  //config.lru_purge_enable = true;

  

  if (httpd_start(&server, &config) == ESP_OK) {

    ESP_LOGI(TAG, "Server ok, registering the URI handlers...");

		// Routes are Registered Here that are visible but must be linked through their uri handlers

    httpd_register_uri_handler(server, &base_uri);

    httpd_register_uri_handler(server, &setAP_uri);

    httpd_register_uri_handler(server, &setSTA_uri);

    return server;

  }

  ESP_LOGI(TAG, "Error starting server");

  return NULL;

}


// Reads the Current Pin Values of active pins and sends it via osc
static void gpio_intr_to_osc(void *arg){
    ESP_LOGI("GPIO", "Read: %d", gpio_get_level(2));
}

// Main
void app_main(void)
{
    // Initialise NVS 
    esp_err_t ret = nvs_flash_init();  
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);



    int AP_MODE = nvs_load_int("Config","Network",-1);
    if ( AP_MODE < AP || AP_MODE >= AP_MODE_END ){
        // First Time boot or error loading default config 
        init_default_Config();
        esp_restart();
    }

    
    /* Initialize event group */
    s_wifi_event_group = xEventGroupCreate();

    /* Register Event handler */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                    ESP_EVENT_ANY_ID,
                    &wifi_event_handler,
                    NULL,
                    NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                    IP_EVENT_STA_GOT_IP,
                    &wifi_event_handler,
                    NULL,
                    NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                    IP_EVENT_ASSIGNED_IP_TO_CLIENT,
                    &wifi_event_handler,
                    NULL,
                    NULL));

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_t *ap = NULL;
    esp_netif_t *sta = NULL;

    if (AP_MODE == AP) {
        ap = esp_netif_create_default_wifi_ap();   // correct place
    } else {
        sta = esp_netif_create_default_wifi_sta(); // correct place
    }


    /* Initialize WiFi */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));


    switch (AP_MODE){
        case AP:
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
            /* Initialize AP */
            ESP_LOGI(TAG_AP, "ESP_WIFI_MODE_AP");
            wifi_init_softap();
            break;
        case STA:            
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
            /* Initialize STA */
            ESP_LOGI(TAG_STA, "ESP_WIFI_MODE_STA");
            wifi_init_sta();
            break;
    }

    
    
    /* Start WiFi */
    ESP_ERROR_CHECK(esp_wifi_start() );

    /*
     * Wait until either the connection is established (WIFI_CONNECTED_BIT) or
     * connection failed for the maximum number of re-tries (WIFI_FAIL_BIT).
     * The bits are set by event_handler() (see above)
     */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned,
     * hence we can test which event actually happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG_STA, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_STA_SSID, EXAMPLE_ESP_WIFI_STA_PASSWD);
        softap_set_dns_addr(ap,sta);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG_STA, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_STA_SSID, EXAMPLE_ESP_WIFI_STA_PASSWD);
    } else {
        ESP_LOGE(TAG_STA, "UNEXPECTED EVENT");
        return;
    }


    // http server 
    httpd_handle_t server = start_webserver();


    // example of what gpio  monitoring looks like

    // task per active pin
    // int active_pins[30] = {-1};




    // for (int i = 0; i < 30; i++){
    //     if (active_pins[i] != -1) {
    //         gpio_intr_enable(active_pins[i]);
    //     }
    // }

    // gpio_intr_enable(GPIO_NUM_2);

    // gpio_isr_register(gpio_intr_to_osc, &active_pins, 1, NULL);    

    // Configure the pin as input

    #define INPUT_PIN GPIO_NUM_2
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << INPUT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    while (1) {
        int level = gpio_get_level(INPUT_PIN);  // Read pin state
        // printf("GPIO %d level: %d\n", INPUT_PIN, level);
        vTaskDelay(pdMS_TO_TICKS(500)); // Delay 500ms
    }
}
