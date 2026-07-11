#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "OscAddress.h"
#include "OscMessage.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "main.h"

#include "esp_system.h"
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "sdkconfig.h"
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
#include "cJSON.h"
#include "OscError.h"
#include "OscPacket.h"
#include "OscSlip.h"
#include "Osc99.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#include "NVS_Helper_Funcs.h"

#define PINCOUNT 28
#define MAX_ATTEMPS 10

#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

/* AP Configuration */
#define EXAMPLE_ESP_WIFI_AP_SSID CONFIG_ESP_WIFI_AP_SSID
#define EXAMPLE_ESP_WIFI_AP_PASSWD CONFIG_ESP_WIFI_AP_PASSWORD
#define EXAMPLE_ESP_WIFI_CHANNEL CONFIG_ESP_WIFI_AP_CHANNEL
#define EXAMPLE_MAX_STA_CONN CONFIG_ESP_MAX_STA_CONN_AP

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

/*DHCP server option*/
#define DHCPS_OFFER_DNS 0x02

static const char *TAG_AP = "WiFi SoftAP";
static const char *TAG_STA = "WiFi Sta";

static int s_retry_num = 0;

// Helper funcs
char *getCurrentIP();

// DefaultS
void init_default_Config()
{

    // Set Default Network Settings
    ESP_ERROR_CHECK(nvs_save_int("Config", "Network", AP));
    // AP
    ESP_ERROR_CHECK(nvs_save_str("AP", "SSID", CONFIG_ESP_WIFI_AP_SSID));
    ESP_ERROR_CHECK(nvs_save_str("AP", "Passphrase", CONFIG_ESP_WIFI_AP_PASSWORD));
    // STA
    ESP_ERROR_CHECK(nvs_save_str("STA", "SSID", CONFIG_ESP_WIFI_REMOTE_AP_SSID));
    ESP_ERROR_CHECK(nvs_save_str("STA", "Passphrase", CONFIG_ESP_WIFI_REMOTE_AP_PASSWORD));

    // Sets OSC message settings
    ESP_ERROR_CHECK(nvs_save_str("OSC", "Remote_IP", CONFIG_ESP_OSC_REMOTE_IP));
    ESP_ERROR_CHECK(nvs_save_int("OSC", "Remote_Port", CONFIG_ESP_OSC_REMOTE_PORT));
    ESP_ERROR_CHECK(nvs_save_str("OSC", "Local_IP", CONFIG_ESP_OSC_LOCAL_IP));
    ESP_ERROR_CHECK(nvs_save_int("OSC", "Local_Port", CONFIG_ESP_OSC_LOCAL_PORT));

    ESP_ERROR_CHECK(nvs_save_int("OSC", "Bundles", 0));
    ESP_ERROR_CHECK(nvs_save_int("OSC", "address_Prefix", 0));

    // GPIO defaults
    for (int i = 1; i < PINCOUNT + 1; i++)
    {

        char key_mode[32];
        char key_io[32];

        snprintf(key_mode, sizeof(key_mode), "Pin-%d-PType", i);
        snprintf(key_io, sizeof(key_io), "Pin-%d-IO", i);

        // Default mode = OFF (0)
        ESP_ERROR_CHECK(nvs_save_int("Pins", key_mode, 0));

        // Default IO = OUTPUT (0)
        ESP_ERROR_CHECK(nvs_save_int("Pins", key_io, 0));
    }
}

/* FreeRTOS event group to signal when we are connected/disconnected */
static EventGroupHandle_t s_wifi_event_group;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG_AP, "Station " MACSTR " joined, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG_AP, "Station " MACSTR " left, AID=%d, reason:%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
        ESP_LOGI(TAG_STA, "Station started");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG_STA, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_ASSIGNED_IP_TO_CLIENT)
    {
        const ip_event_assigned_ip_to_client_t *e = (const ip_event_assigned_ip_to_client_t *)event_data;
        ESP_LOGI(TAG_AP, "Assigned IP to client: " IPSTR ", MAC=" MACSTR ", hostname='%s'",
                 IP2STR(&e->ip), MAC2STR(e->mac), e->hostname);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;

        ESP_LOGW(TAG_STA, "STA disconnected, reason: %d", event->reason);

        if (s_retry_num < CONFIG_ESP_MAXIMUM_STA_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG_STA, "Retrying connection...");
        }
        else
        {
            ESP_LOGE(TAG_STA, "Max retries reached, switching to AP mode");
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    }
}

/* Initialize soft AP */
void wifi_init_softap(void)
{
    char *SSID = nvs_load_str("AP", "SSID", EXAMPLE_ESP_WIFI_AP_SSID);
    char *PASS = nvs_load_str("AP", "Passphrase", EXAMPLE_ESP_WIFI_AP_PASSWD);

    wifi_config_t wifi_ap_config = {0}; // zero-initialise

    // Copy SSID (max 32 bytes)
    size_t ssid_len = strlen(SSID);
    if (ssid_len > 32)
        ssid_len = 32;

    memcpy(wifi_ap_config.ap.ssid, SSID, ssid_len);
    wifi_ap_config.ap.ssid_len = ssid_len;

    // Copy password (max 64 bytes)
    size_t pass_len = strlen(PASS);
    if (pass_len > 64)
        pass_len = 64;

    memcpy(wifi_ap_config.ap.password, PASS, pass_len);

    // Other AP settings
    wifi_ap_config.ap.channel = EXAMPLE_ESP_WIFI_CHANNEL;
    wifi_ap_config.ap.max_connection = EXAMPLE_MAX_STA_CONN;
    wifi_ap_config.ap.authmode = (pass_len == 0) ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    wifi_ap_config.ap.pmf_cfg.required = false;

    // Apply config
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));

    ESP_LOGI(TAG_AP, "SoftAP started. SSID:%s password:%s channel:%d",
             SSID, PASS, EXAMPLE_ESP_WIFI_CHANNEL);

    free(SSID);
    free(PASS);
}

/* Initialize wifi station */
void wifi_init_sta(void)
{
    // esp_netif_t *esp_netif_sta = esp_netif_create_default_wifi_sta();

    char *SSID = nvs_load_str("STA", "SSID", CONFIG_ESP_WIFI_REMOTE_AP_SSID);
    char *PASS = nvs_load_str("STA", "Passphrase", CONFIG_ESP_WIFI_REMOTE_AP_PASSWORD);

    wifi_config_t wifi_sta_config = {
        .sta = {
            .scan_method = WIFI_ALL_CHANNEL_SCAN,
            .failure_retry_cnt = CONFIG_ESP_MAXIMUM_STA_RETRY,
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };

    // Copy SSID (max 32 bytes)
    size_t ssid_len = strlen(SSID);
    if (ssid_len > 32)
        ssid_len = 32;
    memcpy(wifi_sta_config.sta.ssid, SSID, ssid_len);

    // Copy password (max 64 bytes)
    size_t pass_len = strlen(PASS);
    if (pass_len > 64)
        pass_len = 64;
    memcpy(wifi_sta_config.sta.password, PASS, pass_len);

    ESP_LOGE("SSID","%s",SSID);
    ESP_LOGE("PASS","%s",PASS);

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config));

    ESP_LOGI(TAG_STA, "wifi_init_sta finished. SSID:%s PASS:%s",
             SSID, PASS);

    free(SSID);
    free(PASS);

    // return esp_netif_sta;
}

void softap_set_dns_addr(esp_netif_t *esp_netif_ap, esp_netif_t *esp_netif_sta)
{
    esp_netif_dns_info_t dns;
    esp_netif_get_dns_info(esp_netif_sta, ESP_NETIF_DNS_MAIN, &dns);
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

    if (addr.ss_family == AF_INET)
    {
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
    const char *resp_str = "<head><style>tr,th,td {    border:1px solid black;}</style></head><body><script>function updateGPIOFromJSON(data) {    if (!data || !data.pins) {        console.error(\"Invalid GPIO JSON:\", data);        return;    }    const pins = data.pins;    Object.keys(pins).forEach(pinNum => {        const pinData = pins[pinNum];        const modeSelect = document.querySelector(`select[name=\"pin${pinNum}\"]`);        if (modeSelect) {            modeSelect.value = String(pinData.mode);        }        const ioSelect = document.querySelector(`select[name=\"pin${pinNum}-io\"]`);        if (ioSelect) {            ioSelect.value = String(pinData.io);        }    });}function updateNetworkFromJSON(data) {    if (!data) {        console.error(\"Invalid Network JSON:\", data);        return;    }        const modeAP  = document.querySelector('input[name=\"mode\"][value=\"AP\"]');    const modeSTA = document.querySelector('input[name=\"mode\"][value=\"STA\"]');    if (data.mode === \"AP\" && modeAP)  modeAP.checked = true;    if (data.mode === \"STA\" && modeSTA) modeSTA.checked = true;        const apSSID = document.querySelector('input[name=\"AP-SSID\"]');    const apPass = document.querySelector('input[name=\"AP-Password\"]');    if (apSSID) apSSID.value = data.AP_SSID || \"\";    if (apPass) apPass.value = data.AP_Password || \"\";        const staSSID = document.querySelector('input[name=\"STA-SSID\"]');    const staPass = document.querySelector('input[name=\"STA-Password\"]');    if (staSSID) staSSID.value = data.STA_SSID || \"\";    if (staPass) staPass.value = data.STA_Password || \"\";}function updateOSCFromJSON(data) {    if (!data) {        console.error(\"Invalid OSC JSON:\", data);        return;    }    const remoteIP   = document.querySelector('input[name=\"OSC-Remote\"]');    const remotePort = document.querySelector('input[name=\"OSC-Remote-Port\"]');    const localIP    = document.querySelector('input[name=\"OSC-Local\"]');    const localPort  = document.querySelector('input[name=\"OSC-Local-Port\"]');    if (remoteIP)   remoteIP.value   = data.remote_ip   || \"\";    if (remotePort) remotePort.value = data.remote_port || \"\";    if (localIP)    localIP.value    = data.local_ip    || \"\";    if (localPort)  localPort.value  = data.local_port  || \"\";}document.addEventListener(\"DOMContentLoaded\", () => {    fetch(\"/gpio.json\")        .then(res => res.json())        .then(json => updateGPIOFromJSON(json))        .catch(err => console.error(\"Failed to load GPIO JSON:\", err));            fetch(\"/network.json\")        .then(res => res.json())        .then(json => updateNetworkFromJSON(json))        .catch(err => console.error(\"Failed to load Network JSON:\", err));        fetch(\"/osc.json\")        .then(res => res.json())        .then(json => updateOSCFromJSON(json))        .catch(err => console.error(\"Failed to load OSC JSON:\", err));});</script><section>    <h1>Network</h1>    <form method=\"get\" action=\"/network\">        <p>Network Type:</p>    <input type=\"radio\" name=\"mode\" value=\"AP\"> Self Host    <input type=\"radio\" name=\"mode\" value=\"STA\"> Join Network    <br>    <!-- AP -->    <label for=\"AP-SSID\">SSID</label>    <input type=\"text\" name=\"AP-SSID\">    <br>    <label for=\"AP-Password\">Password</label>    <input type=\"text\" name=\"AP-Password\">    <br>    <!-- STA -->    <label for=\"STA-SSID\">SSID</label>    <input type=\"text\" name=\"STA-SSID\">    <br>    <label for=\"STA-Password\">Password</label>    <input type=\"text\" name=\"STA-Password\">    <br>    <button type=\"submit\" style=\"column-span: 3;\">Update Network Configuration</button>            </form>    </section><section>    <h1>OSC</h1>    <form method=\"get\" action=\"/OSC\">    <!-- OSC -->    <label for=\"OSC-Remote\">Remote IP</label>    <input type=\"text\" name=\"OSC-Remote\">    <label for=\"OSC-Remote-Port\">Port:</label>    <input type=\"number\" name=\"OSC-Remote-Port\">    <br>    <label for=\"OSC-Local\">Local IP</label>    <input type=\"text\" name=\"OSC-Local\">    <label for=\"OSC-Local-Port\">Port:</label>    <input type=\"number\" name=\"OSC-Local-Port\">    <br>    <button type=\"submit\" style=\"column-span: 3;\">Update OSC Configuration</button>            </form>    </section><section><h1>GPIO</h1>    <form method=\"post\" action=\"/GPIO\"> <table><tr><td>Pin - 1</td><td><select name=\"pin1\"> <option value=\"0\">OFF</option><option value=\"1\">Analog</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin1-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 2</td><td><select name=\"pin2\"> <option value=\"0\">OFF</option><option value=\"1\">Analog</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin2-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 3</td><td><select name=\"pin3\"> <option value=\"0\">OFF</option><option value=\"1\">Analog</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin3-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 4</td><td><select name=\"pin4\"> <option value=\"0\">OFF</option><option value=\"1\">Analog</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin4-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 5</td><td><select name=\"pin5\"> <option value=\"0\">OFF</option><option value=\"1\">Analog</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin5-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 6</td><td><select name=\"pin6\"> <option value=\"0\">OFF</option><option value=\"1\">Analog</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin6-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 7</td><td><select name=\"pin7\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin7-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 8</td><td><select name=\"pin8\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin8-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 9</td><td><select name=\"pin9\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin9-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 10</td><td><select name=\"pin10\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin10-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 11</td><td><select name=\"pin11\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin11-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 12</td><td><select name=\"pin12\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin12-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 13</td><td><select name=\"pin13\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin13-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 14</td><td><select name=\"pin14\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin14-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 15</td><td><select name=\"pin15\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin15-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 16</td><td><select name=\"pin16\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin16-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 17</td><td><select name=\"pin17\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin17-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 18</td><td><select name=\"pin18\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin18-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 19</td><td><select name=\"pin19\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin19-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 20</td><td><select name=\"pin20\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin20-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 21</td><td><select name=\"pin21\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin21-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 22</td><td><select name=\"pin22\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin22-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 23</td><td><select name=\"pin23\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin23-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 24</td><td><select name=\"pin24\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin24-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 25</td><td><select name=\"pin25\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin25-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 26</td><td><select name=\"pin26\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin26-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 27</td><td><select name=\"pin27\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin27-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 28</td><td><select name=\"pin28\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin28-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr></table><button type=\"submit\">Save </button></form></body>   ";
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t Conf_Reset(httpd_req_t *req)
{
    ESP_ERROR_CHECK(nvs_save_int("Config", "Network", -1));
    esp_restart();
    return ESP_OK;
}

static esp_err_t Network_Handler(httpd_req_t *req)
{
    char query[256];
    char value[16];

    size_t qlen = httpd_req_get_url_query_len(req);
    if (qlen == 0 || qlen >= sizeof(query))
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing query");
    }

    // Read query string
    httpd_req_get_url_query_str(req, query, sizeof(query));

    char mode[8];

    if (httpd_query_key_value(query, "mode", mode, sizeof(mode)) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing mode");

    bool isAP = strcmp(mode, "AP") == 0;
    bool isSTA = strcmp(mode, "STA") == 0;

    if (isAP)
    {
        ESP_ERROR_CHECK(nvs_save_int("Config", "Network", AP));
    }
    else if (isSTA)
    {
        ESP_ERROR_CHECK(nvs_save_int("Config", "Network", STA));
    }
    else
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid mode");
    }

    // --- Read AP SSID (string) ---
    if (httpd_query_key_value(query, "AP-SSID", value, sizeof(value)) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing AP-SSID");

    char AP_SSID[64];
    strcpy(AP_SSID, value);

    // --- Read AP Password (string) ---
    if (httpd_query_key_value(query, "AP-Password", value, sizeof(value)) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing AP-Password");

    char AP_Password[64];
    strcpy(AP_Password, value);

    // --- Read STA SSID (string) ---
    if (httpd_query_key_value(query, "STA-SSID", value, sizeof(value)) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing STA-SSID");

    char STA_SSID[64];
    strcpy(STA_SSID, value);

    // --- Read STA Password (string) ---
    if (httpd_query_key_value(query, "STA-Password", value, sizeof(value)) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing STA-Password");

    char STA_Password[64];
    strcpy(STA_Password, value);

    ESP_ERROR_CHECK(nvs_save_str("AP", "SSID", AP_SSID));
    ESP_ERROR_CHECK(nvs_save_str("AP", "Passphrase", AP_Password));
    // STA
    ESP_ERROR_CHECK(nvs_save_str("STA", "SSID", STA_SSID));
    ESP_ERROR_CHECK(nvs_save_str("STA", "Passphrase", STA_Password));

    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);

    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();

    return ESP_OK;
};

static esp_err_t OSC_Handler(httpd_req_t *req)
{
    char query[256];
    char value[64];

    size_t qlen = httpd_req_get_url_query_len(req);
    if (qlen == 0 || qlen >= sizeof(query))
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing query");
    }

    httpd_req_get_url_query_str(req, query, sizeof(query));

    // --- Remote IP ---
    if (httpd_query_key_value(query, "OSC-Remote", value, sizeof(value)) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing OSC-Remote");

    char remote_ip[64];
    strcpy(remote_ip, value);

    // --- Remote Port ---
    if (httpd_query_key_value(query, "OSC-Remote-Port", value, sizeof(value)) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing OSC-Remote-Port");

    int remote_port = atoi(value);

    // --- Local IP ---
    if (httpd_query_key_value(query, "OSC-Local", value, sizeof(value)) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing OSC-Local");

    char local_ip[64];
    strcpy(local_ip, value);

    // --- Local Port ---
    if (httpd_query_key_value(query, "OSC-Local-Port", value, sizeof(value)) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing OSC-Local-Port");

    int local_port = atoi(value);

    // --- Save to NVS ---
    ESP_ERROR_CHECK(nvs_save_str("OSC", "Remote_IP", remote_ip));
    ESP_ERROR_CHECK(nvs_save_int("OSC", "Remote_Port", remote_port));

    ESP_ERROR_CHECK(nvs_save_str("OSC", "Local_IP", local_ip));
    ESP_ERROR_CHECK(nvs_save_int("OSC", "Local_Port", local_port));

    // Respond immediately so browser stops loading
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, "OSC Saved", HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
};

static esp_err_t GPIO_Handler(httpd_req_t *req)
{
    // --- Read POST body ---
    int total = req->content_len;
    if (total <= 0 || total > 1024)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid body");
    }

    char body[1024];
    int received = httpd_req_recv(req, body, sizeof(body) - 1);

    if (received <= 0)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read body");
    }

    body[received] = '\0'; // Null‑terminate

    // --- Parse all pins ---
    char value[32];

    for (int i = 1; i <= 28; i++)
    {

        char field_mode[16];
        char field_io[16];

        char key_mode[32];
        char key_io[32];

        // HTML field names
        snprintf(field_mode, sizeof(field_mode), "pin%d", i);
        snprintf(field_io, sizeof(field_io), "pin%d-io", i);

        // NVS keys
        snprintf(key_mode, sizeof(key_mode), "Pin-%d-PType", i);
        snprintf(key_io, sizeof(key_io), "Pin-%d-IO", i);

        // --- MODE ---
        if (httpd_query_key_value(body, field_mode, value, sizeof(value)) == ESP_OK)
        {
            int mode = atoi(value);
            if (mode < 0 || mode > 2)
                mode = 0;
            ESP_ERROR_CHECK(nvs_save_int("Pins", key_mode, mode));
        }

        // --- IO ---
        if (httpd_query_key_value(body, field_io, value, sizeof(value)) == ESP_OK)
        {
            int io = atoi(value);
            if (io < 0 || io > 1)
                io = 0;
            ESP_ERROR_CHECK(nvs_save_int("Pins", key_io, io));
        }
    }

    // --- Respond so browser stops loading ---
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, "GPIO Saved", HTTPD_RESP_USE_STRLEN);

    // Allow response to flush
    vTaskDelay(pdMS_TO_TICKS(100));

    return ESP_OK;
};

static esp_err_t gpio_json_handler(httpd_req_t *req)
{
    char json[2048];
    int offset = 0;

    offset += snprintf(json + offset, sizeof(json) - offset, "{ \"pins\": {");

    for (int i = 1; i <= 28; i++)
    {

        // Build NVS keys
        char key_mode[32];
        char key_io[32];

        snprintf(key_mode, sizeof(key_mode), "Pin-%d-PType", i);
        snprintf(key_io, sizeof(key_io), "Pin-%d-IO", i);

        // Load values
        int mode = nvs_load_int("Pins", key_mode, 0);
        int io = nvs_load_int("Pins", key_io, 0);

        // Append JSON entry
        offset += snprintf(json + offset, sizeof(json) - offset,
                           "\"%d\": {\"mode\": %d, \"io\": %d}%s",
                           i, mode, io,
                           (i < 28 ? "," : ""));
    }

    offset += snprintf(json + offset, sizeof(json) - offset, "} }");

    // Send JSON
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));

    return ESP_OK;
}

static esp_err_t network_json_handler(httpd_req_t *req)
{
    char json[512];
    int offset = 0;

    // Load mode
    int mode = nvs_load_int("Config", "Network", AP);

    // Load AP settings
    char *ap_ssid = nvs_load_str("AP", "SSID", "");
    char *ap_pass = nvs_load_str("AP", "Passphrase", "");

    // Load STA settings
    char *sta_ssid = nvs_load_str("STA", "SSID", "");
    char *sta_pass = nvs_load_str("STA", "Passphrase", "");

    // Build JSON
    offset += snprintf(json + offset, sizeof(json) - offset,
                       "{"
                       "\"mode\":\"%s\","
                       "\"AP_SSID\":\"%s\","
                       "\"AP_Password\":\"%s\","
                       "\"STA_SSID\":\"%s\","
                       "\"STA_Password\":\"%s\""
                       "}",
                       (mode == AP ? "AP" : "STA"),
                       ap_ssid,
                       ap_pass,
                       sta_ssid,
                       sta_pass);

    // Free allocated strings
    free(ap_ssid);
    free(ap_pass);
    free(sta_ssid);
    free(sta_pass);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));

    return ESP_OK;
}

static esp_err_t osc_json_handler(httpd_req_t *req)
{
    char json[256];
    int offset = 0;

    // Load OSC settings
    char *remote_ip = nvs_load_str("OSC", "Remote_IP", "0.0.0.0");
    // char *local_ip  = nvs_load_str("OSC", "Local_IP",  "0.0.0.0");
    char *local_ip = getCurrentIP();

    int remote_port = nvs_load_int("OSC", "Remote_Port", 9000);
    int local_port = nvs_load_int("OSC", "Local_Port", 8000);

    // Build JSON
    offset += snprintf(json + offset, sizeof(json) - offset,
                       "{"
                       "\"remote_ip\":\"%s\","
                       "\"remote_port\":%d,"
                       "\"local_ip\":\"%s\","
                       "\"local_port\":%d"
                       "}",
                       remote_ip,
                       remote_port,
                       local_ip,
                       local_port);

    // Free allocated strings
    free(remote_ip);
    // free(local_ip);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));

    return ESP_OK;
}

static const httpd_uri_t base_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = base_handler,
    .user_ctx = NULL};

static const httpd_uri_t reset_uri = {
    .uri = "/reset",
    .method = HTTP_GET,
    .handler = Conf_Reset,
    .user_ctx = NULL};

static const httpd_uri_t network_uri = {
    .uri = "/network",
    .method = HTTP_GET,
    .handler = Network_Handler,
    .user_ctx = NULL,
};

static const httpd_uri_t OSC_uri = {
    .uri = "/OSC",
    .method = HTTP_GET,
    .handler = OSC_Handler,
    .user_ctx = NULL,
};
static const httpd_uri_t GPIO_uri = {
    .uri = "/GPIO",
    .method = HTTP_POST,
    .handler = GPIO_Handler,
    .user_ctx = NULL,
};

static const httpd_uri_t gpio_json_uri = {
    .uri = "/gpio.json",
    .method = HTTP_GET,
    .handler = gpio_json_handler,
    .user_ctx = NULL};

static const httpd_uri_t osc_json_uri = {
    .uri = "/osc.json",
    .method = HTTP_GET,
    .handler = osc_json_handler,
    .user_ctx = NULL};

static const httpd_uri_t network_json_uri = {
    .uri = "/network.json",
    .method = HTTP_GET,
    .handler = network_json_handler,
    .user_ctx = NULL};

// Defines the Full Http server
httpd_handle_t start_webserver()
{

    httpd_handle_t server = NULL;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // config.lru_purge_enable = true;

    if (httpd_start(&server, &config) == ESP_OK)
    {

        ESP_LOGI(TAG, "Server ok, registering the URI handlers...");

        // Routes are Registered Here that are visible but must be linked through their uri handlers

        httpd_register_uri_handler(server, &base_uri);

        httpd_register_uri_handler(server, &network_uri);

        httpd_register_uri_handler(server, &OSC_uri);

        httpd_register_uri_handler(server, &GPIO_uri);

        httpd_register_uri_handler(server, &gpio_json_uri);

        httpd_register_uri_handler(server, &osc_json_uri);

        httpd_register_uri_handler(server, &network_json_uri);

        httpd_register_uri_handler(server, &reset_uri);

        return server;
    }

    ESP_LOGI(TAG, "Error starting server");

    return NULL;
}

// UDP

char *getCurrentIP()
{
    static char ip_str[16];
    esp_netif_ip_info_t ip_info;

    int AP_MODE = nvs_load_int("Config", "Network", -1);

    esp_netif_t *netif = NULL;

    if (AP_MODE == STA)
    {
        netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    }
    else if (AP_MODE == AP)
    {
        netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    }

    if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK)
    {
        sprintf(ip_str, IPSTR, IP2STR(&ip_info.ip));
        return ip_str;
    }

    return "0.0.0.0";
}

void sendPingMessage();
// OSC message server
void ProcessMessage(const OscTimeTag *const oscTimeTag,
                    OscMessage *const oscMessage)
{
    const char *addr = oscMessage->oscAddressPattern;
    ESP_LOGI("OSC_Process", "%s", addr);
    // Match prefix

    if (OscAddressMatch(addr, "/ping"))
    {
        sendPingMessage();
    }

    if (OscAddressMatch(addr, "/outputs/digital/6"))
    {
        ESP_LOGI("OSC_Proccess", "addr");
        // Extract channel number
        int gpio = atoi(addr + strlen("/outputs/digital/"));

        // Extract integer argument
        int32_t level;
        if (OscMessageGetArgumentAsInt32(oscMessage, &level) != OscErrorNone)
        {
            return;
        }

        // Configure pin
        gpio_config_t cfg = {
            .pin_bit_mask = 1ULL << gpio,
            .mode = GPIO_MODE_OUTPUT,
        };
        gpio_config(&cfg);

        // Set pin
        gpio_set_level(gpio, level ? 1 : 0);
        return;
    }
}

/* UDP socket tests */

#define PORT 3333
static int sock = -1;
static struct sockaddr_storage last_client_addr;
static socklen_t last_client_len = 0;

void sendPingMessage();

static void udp_server_task(void *pvParameters)
{
    char rx_buffer[128];
    char addr_str[128];
    int addr_family = (int)pvParameters;
    int ip_protocol = 0;
    struct sockaddr_in6 dest_addr;

    // Load local bind port from NVS
    int32_t local_port = nvs_load_int("OSC", "Local_Port", 3333);

    // Load remote IP + port from NVS (used for sending)
    char *remote_ip = nvs_load_str("OSC", "Remote_IP", "0.0.0.0");
    int32_t remote_port = nvs_load_int("OSC", "Remote_Port", 10000);

    // Build last_client_addr from NVS values (IPv4 only)
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(remote_port);

    if (inet_aton(remote_ip, &client_addr.sin_addr) == 0)
    {
        ESP_LOGE(TAG, "Invalid Remote_IP in NVS: %s", remote_ip);
    }

    free(remote_ip);

    memcpy(&last_client_addr, &client_addr, sizeof(client_addr));
    last_client_len = sizeof(client_addr);

    while (1)
    {

        // Build local bind address
        if (addr_family == AF_INET)
        {
            struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
            memset(dest_addr_ip4, 0, sizeof(struct sockaddr_in));
            dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
            dest_addr_ip4->sin_family = AF_INET;
            dest_addr_ip4->sin_port = htons(local_port);
            ip_protocol = IPPROTO_IP;
        }
        else
        {
            memset(&dest_addr, 0, sizeof(dest_addr));
            dest_addr.sin6_family = AF_INET6;
            dest_addr.sin6_port = htons(local_port);
            ip_protocol = IPPROTO_IPV6;
        }

        sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0)
        {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }

        ESP_LOGI(TAG, "Socket created");

        struct timeval timeout = {.tv_sec = 10, .tv_usec = 0};
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0)
        {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        }

        ESP_LOGI(TAG, "Socket bound, port %d", local_port);

        struct sockaddr_storage source_addr;
        socklen_t socklen = sizeof(source_addr);

        while (1)
        {
            ESP_LOGI(TAG, "Waiting for data");

            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0,
                               (struct sockaddr *)&source_addr, &socklen);

            if (len < 0)
            {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                break;
            }

            // Convert sender IP only for logging
            if (source_addr.ss_family == PF_INET)
            {
                inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr,
                            addr_str, sizeof(addr_str));
            }
            else
            {
                inet6_ntoa_r(((struct sockaddr_in6 *)&source_addr)->sin6_addr,
                             addr_str, sizeof(addr_str));
            }

            rx_buffer[len] = 0;
            ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
            ESP_LOGI(TAG, "%s", rx_buffer);

            // Process OSC
            OscPacket oscPacket;
            OscPacketInitialiseFromCharArray(&oscPacket, rx_buffer, len);
            oscPacket.processMessage = ProcessMessage;
            OscPacketProcessMessages(&oscPacket);
        }

        shutdown(sock, 0);
        close(sock);
    }

    vTaskDelete(NULL);
}

void udp_send_osc(OscPacket msg)
{
    if (sock < 0)
    {
        ESP_LOGE(TAG, "Socket not initialized");
        return;
    }

    char *ip_string = nvs_load_str("OSC", "Remote_IP","192.168.4.2");

    const int port_num = nvs_load_int("OSC", "Remote_Port", 8000);

    ESP_LOGI("UDP_REMOTE_IP","Value: %s",ip_string);
    ESP_LOGI("UDP_REMOTE_PORT","%d",port_num);

    const struct sockaddr_in dest_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port_num),
        .sin_addr.s_addr = inet_addr(ip_string),
    };

    int err = sendto(
        sock,
        msg.contents,
        msg.size,
        0,
        (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        

    if (err < 0)
    {
        ESP_LOGE(TAG, "Send failed: errno %d", errno);
    }
    else
    {
        // ESP_LOGI(TAG, "Sent: %s", msg);
    }
    free(ip_string);
}

// OSC
void sendOscContents(const void *const oscContents)
{
    OscPacket OscPacket;
    if (OscPacketInitialiseFromContents(&OscPacket, oscContents) != OscErrorNone)
    {
        return;
    }

    // encode a slip packet
    // char slipPacket[MAX_OSC_PACKET_SIZE];
    // size_t slipPacketSize;
    // if (OscSlipEncodePacket(&OscPacket, &slipPacketSize, slipPacket, sizeof(slipPacket))){
    //     return;
    // }

    // send Packet

    udp_send_osc(OscPacket);
}

void sendPingMessage()
{
    OscMessage oscMessage;
    OscMessageInitialise(&oscMessage, "/ping");

    // 1. Add current IP (AP or STA depending on NVS setting)
    OscMessageAddString(&oscMessage, getCurrentIP());

    // 2. Add MAC address
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    char mac_str[18];
    sprintf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    OscMessageAddString(&oscMessage, mac_str);

    // 3. Add firmware version
    // You can replace this with your own version string
    OscMessageAddString(&oscMessage, "0.0.1");

    // Send the OSC message
    sendOscContents(&oscMessage);
}

void oscDigitalSend(int pinval, int pinnum) {
};

void oscAnalogueSend(float pinval, int pinnum);

// Pin reads
adc_oneshot_unit_handle_t adc_handle;

void adc_init(void)
{
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc_handle));

    for (int i = 2; i < 6 + 1; i++)
    {

        int channel = i - 1;

        adc_oneshot_chan_cfg_t chan_cfg = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };

        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, channel, &chan_cfg));
    }
}

int readDigitalPin(int pin)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};

    gpio_config(&io_conf);

    return gpio_get_level(pin);
}

float readAnaloguePin(int pin)
{
    int channel = pin - 1;

    int raw = 0;
    ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, channel, &raw));

    return (float)raw / 4095.0f;
}

// Reads the Current Pin Values of active pins and sends it via osc

typedef enum GPIO_STATE
{
    OFF,
    ANALOGUE,
    DIGITAL,
    END,
} GPIO_STATE;

static int last_digital[PINCOUNT] = {0};

void send_digital_inputs(void)
{
    int changed = 0;
    int values[PINCOUNT];

    for (int i = 1; i < PINCOUNT + 1; i++)
    {

        char key_mode[32];
        snprintf(key_mode, sizeof(key_mode), "Pin-%d-PType", i);

        int mode = nvs_load_int("Pins", key_mode, 0);

        if (mode == DIGITAL)
        {
            int val = readDigitalPin(i);
            values[i - 1] = val;

            if (val != last_digital[i - 1])
            {
                changed = 1;
                last_digital[i - 1] = val;
            }
        }
        else
        {
            values[i - 1] = 0;
        }
    }

    if (!changed)
        return;

    OscMessage msg;
    OscMessageInitialise(&msg, "/inputs/digital");

    for (int i = 0; i < PINCOUNT; i++)
    {
        OscMessageAddInt32(&msg, values[i]);
    }

    sendOscContents(&msg);
}

void send_analogue_inputs(void)
{
    OscMessage msg;
    OscMessageInitialise(&msg, "/inputs/analogue");

    for (int i = 1; i < PINCOUNT + 1; i++)
    {

        char key_mode[32];
        snprintf(key_mode, sizeof(key_mode), "Pin-%d-PType", i);

        int mode = nvs_load_int("Pins", key_mode, 0);

        if (mode == ANALOGUE)
        {
            float val = readAnaloguePin(i);
            OscMessageAddFloat32(&msg, val);
        }
        else
        {
            OscMessageAddFloat32(&msg, 0.0f);
        }
    }

    sendOscContents(&msg);
}

void gpio_task(void *pv)
{
    while (1)
    {

        send_digital_inputs();  // only sends on change
        send_analogue_inputs(); // sends every cycle

        vTaskDelay(pdMS_TO_TICKS(10)); // 100 Hz
    }
}

void app_main(void)
{
    // NVS init (unchanged)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);


    
    ESP_ERROR_CHECK(nvs_save_int("test", "count", 123));
    int32_t v = nvs_load_int("test", "count", -1);
    ESP_LOGI("NVS", "Loaded count = %" PRId32, v);


    int AP_MODE = nvs_load_int("Config", "Network", -1);
    if (AP_MODE < AP || AP_MODE >= AP_MODE_END)
    {
        init_default_Config();
        esp_restart();
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_t *ap = NULL;
    esp_netif_t *sta = NULL;

    if (AP_MODE == AP)
    {
        ap = esp_netif_create_default_wifi_ap();
    }
    else
    { // STA
        sta = esp_netif_create_default_wifi_sta();
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    if (AP_MODE == AP)
    {
        // --- AP ONLY PATH ---
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_LOGI(TAG_AP, "ESP_WIFI_MODE_AP");
        wifi_init_softap();
        ESP_ERROR_CHECK(esp_wifi_start());

        // No event group wait here
        (void)ap; // if unused for now

        // Start HTTP server directly
        httpd_handle_t server = start_webserver();
        (void)server;
    }
    else
    {
        // --- STA PATH ---
        // Create event group only for STA
        s_wifi_event_group = xEventGroupCreate();
        assert(s_wifi_event_group != NULL);

        // Register handlers (STA-related)
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &wifi_event_handler,
            NULL,
            NULL));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            &wifi_event_handler,
            NULL,
            NULL));

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_LOGI(TAG_STA, "ESP_WIFI_MODE_STA");
        wifi_init_sta();
        ESP_ERROR_CHECK(esp_wifi_start());

        EventBits_t bits = xEventGroupWaitBits(
            s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

        if (bits & WIFI_CONNECTED_BIT)
        {
            ESP_LOGI(TAG_STA, "connected to ap SSID:%s password:%s",
                     CONFIG_ESP_WIFI_REMOTE_AP_SSID, CONFIG_ESP_WIFI_REMOTE_AP_PASSWORD);
            // If you ever run AP+STA, only then call:
            // softap_set_dns_addr(ap, sta);
        }
        else if (bits & WIFI_FAIL_BIT)
        {
            ESP_LOGE(TAG_STA, "Failed to connect to SSID:%s, password:%s",
                     CONFIG_ESP_WIFI_REMOTE_AP_SSID, CONFIG_ESP_WIFI_REMOTE_AP_PASSWORD);
            ESP_ERROR_CHECK(nvs_save_int("Config", "Network", AP));
            esp_restart();
        }
        else
        {
            ESP_LOGE(TAG_STA, "UNEXPECTED EVENT");
        }

        // Optionally start HTTP server here if you want it in STA mode too
        httpd_handle_t server = start_webserver();
        (void)server;
    }

#ifdef CONFIG_EXAMPLE_IPV4
    xTaskCreate(udp_server_task, "udp_server", 12288, (void *)AF_INET, 5, NULL);
#endif
#ifdef CONFIG_EXAMPLE_IPV6
    xTaskCreate(udp_server_task, "udp_server", 4096, (void *)AF_INET6, 5, NULL);
#endif


    adc_init();

    xTaskCreate(
        gpio_task,   // Task function
        "GPIO Task", // Name (for debugging)
        8192,        // Stack size in bytes
        NULL,        // Task parameters
        5,           // Priority (1–10 typical)
        NULL         // Task handle (optional)
    );
}
