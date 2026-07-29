#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "OscAddress.h"
#include "OscMessage.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"
// #include "main.h"

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
#include "OSC_Routes.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#include "esp_log.h"

// #include "global_nvs.h"
#include "NVS_Helper_Funcs.h"
#include "Networking.h"


#define PINCOUNT CONFIG_PINCOUNT
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

#define FIRMWARE_VERSION CONFIG_FIRMWARE_VERSION

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1


static const char *TAG_AP = "WiFi SoftAP";
static const char *TAG_STA = "WiFi Sta";

static int s_retry_num = 0;

// NVS_GLOBAL_INTERMIDARY

static NVS_Global nvs_global;

static const NVS_Global NVS_DEFAULTS = {
    .net_settings = {
        .network_mode = 0,
        .ap_ssid = "MyAP",
        .ap_password = "password",
        .sta_ssid = "",
        .sta_password = "",
    },
    .osc_settings = {
        .remote_ip = "192.168.1.10",
        .remote_port = 9000,
        .local_ip = "0.0.0.0",
        .local_port = 9001,
        .bundle = false,
        .address_prefix = false,
    },
    .gpio_settings = {
        .gpio_rate = 1000,
        .pin_mode = {GPIO_OFF},
        .pin_io = {GPIO_OUTPUT},
    }};

// Helper funcs
char *getCurrentIP();

// Defaults
void init_default_Config(NVS_Global *nvs)
{
    /* -----------------------------------------
       Network Settings
       ----------------------------------------- */

    uint32_t mode = AP;
    ESP_ERROR_CHECK(nvs_update(nvs, "net_settings.network_mode", &mode));

    ESP_ERROR_CHECK(nvs_update(nvs, "net_settings.ap_ssid", CONFIG_ESP_WIFI_AP_SSID));
    ESP_ERROR_CHECK(nvs_update(nvs, "net_settings.ap_password", CONFIG_ESP_WIFI_AP_PASSWORD));

    ESP_ERROR_CHECK(nvs_update(nvs, "net_settings.sta_ssid", CONFIG_ESP_WIFI_REMOTE_AP_SSID));
    ESP_ERROR_CHECK(nvs_update(nvs, "net_settings.sta_password", CONFIG_ESP_WIFI_REMOTE_AP_PASSWORD));

    /* -----------------------------------------
       OSC Settings
       ----------------------------------------- */

    ESP_ERROR_CHECK(nvs_update(nvs, "osc_settings.remote_ip", CONFIG_ESP_OSC_REMOTE_IP));

    uint16_t remote_port = CONFIG_ESP_OSC_REMOTE_PORT;
    ESP_ERROR_CHECK(nvs_update(nvs, "osc_settings.remote_port", &remote_port));

    ESP_ERROR_CHECK(nvs_update(nvs, "osc_settings.local_ip", CONFIG_ESP_OSC_LOCAL_IP));

    uint16_t local_port = CONFIG_ESP_OSC_LOCAL_PORT;
    ESP_ERROR_CHECK(nvs_update(nvs, "osc_settings.local_port", &local_port));

    bool bundles = false;
    ESP_ERROR_CHECK(nvs_update(nvs, "osc_settings.bundle", &bundles));

    bool prefix = false;
    ESP_ERROR_CHECK(nvs_update(nvs, "osc_settings.address_prefix", &prefix));

    // GPIO defaults
    uint32_t rate = 100;
    ESP_ERROR_CHECK(nvs_update(nvs, "gpio_settings.gpio_rate", &rate));

    // Pin defaults: all pins OFF / OUTPUT, saved as two blobs
    GPIO_State default_modes[PINCOUNT];
    GPIO_IO default_ios[PINCOUNT];
    for (int i = 0; i < PINCOUNT; i++)
    {
        default_modes[i] = GPIO_OFF;
        default_ios[i] = GPIO_OUTPUT;
    }

    ESP_ERROR_CHECK(nvs_save_gpio_pins(default_modes, default_ios));
}

/* FreeRTOS event group to signal when we are connected/disconnected */
static EventGroupHandle_t s_wifi_event_group;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG_AP, "Station " MACSTR " joined, AID=%d", MAC2STR(event->mac), event->aid);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG_AP, "Station " MACSTR " left, AID=%d, reason:%d", MAC2STR(event->mac), event->aid, event->reason);
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
        ESP_LOGI(TAG_AP, "Assigned IP to client: " IPSTR ", MAC=" MACSTR ", hostname='%s'", IP2STR(&e->ip), MAC2STR(e->mac), e->hostname);
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

/* An HTTP GET handler */
static esp_err_t base_handler(httpd_req_t *req)
{
    print_all_nvs_entries("Global-Config");
    const char *resp_str = "<head><style>tr,th,td {    border:1px solid black;}</style></head><body><script>function updateGPIOFromJSON(data) {    if (!data || !data.pins) {        console.error(\"Invalid GPIO JSON:\", data);        return;    }    const pins = data.pins;    Object.keys(pins).forEach(pinNum => {        const pinData = pins[pinNum];        const modeSelect = document.querySelector(`select[name=\"pin${pinNum}\"]`);        if (modeSelect) {            modeSelect.value = String(pinData.mode);        }        const ioSelect = document.querySelector(`select[name=\"pin${pinNum}-io\"]`);        if (ioSelect) {            ioSelect.value = String(pinData.io);        }    });}function updateNetworkFromJSON(data) {    if (!data) {        console.error(\"Invalid Network JSON:\", data);        return;    }        const modeAP  = document.querySelector('input[name=\"mode\"][value=\"AP\"]');    const modeSTA = document.querySelector('input[name=\"mode\"][value=\"STA\"]');    if (data.mode === \"AP\" && modeAP)  modeAP.checked = true;    if (data.mode === \"STA\" && modeSTA) modeSTA.checked = true;        const apSSID = document.querySelector('input[name=\"AP-SSID\"]');    const apPass = document.querySelector('input[name=\"AP-Password\"]');    if (apSSID) apSSID.value = data.AP_SSID || \"\";    if (apPass) apPass.value = data.AP_Password || \"\";        const staSSID = document.querySelector('input[name=\"STA-SSID\"]');    const staPass = document.querySelector('input[name=\"STA-Password\"]');    if (staSSID) staSSID.value = data.STA_SSID || \"\";    if (staPass) staPass.value = data.STA_Password || \"\";}function updateOSCFromJSON(data) {    if (!data) {        console.error(\"Invalid OSC JSON:\", data);        return;    }    const remoteIP   = document.querySelector('input[name=\"OSC-Remote\"]');    const remotePort = document.querySelector('input[name=\"OSC-Remote-Port\"]');    const localIP    = document.querySelector('input[name=\"OSC-Local\"]');    const localPort  = document.querySelector('input[name=\"OSC-Local-Port\"]');    if (remoteIP)   remoteIP.value   = data.remote_ip   || \"\";    if (remotePort) remotePort.value = data.remote_port || \"\";    if (localIP)    localIP.value    = data.local_ip    || \"\";    if (localPort)  localPort.value  = data.local_port  || \"\";}document.addEventListener(\"DOMContentLoaded\", () => {    fetch(\"/gpio.json\")        .then(res => res.json())        .then(json => updateGPIOFromJSON(json))        .catch(err => console.error(\"Failed to load GPIO JSON:\", err));            fetch(\"/network.json\")        .then(res => res.json())        .then(json => updateNetworkFromJSON(json))        .catch(err => console.error(\"Failed to load Network JSON:\", err));        fetch(\"/osc.json\")        .then(res => res.json())        .then(json => updateOSCFromJSON(json))        .catch(err => console.error(\"Failed to load OSC JSON:\", err));});</script><section>    <h1>Network</h1>    <form method=\"get\" action=\"/network\">        <p>Network Type:</p>    <input type=\"radio\" name=\"mode\" value=\"AP\"> Self Host    <input type=\"radio\" name=\"mode\" value=\"STA\"> Join Network    <br>    <!-- AP -->    <h3> Access Point Mode (Self Host) </h3>    <label for=\"AP-SSID\">SSID</label>    <input type=\"text\" name=\"AP-SSID\">    <br>    <label for=\"AP-Password\">Password</label>    <input type=\"text\" name=\"AP-Password\">    <br>    <!-- STA -->    <h3> Station Mode (Join Network) </h3>    <label for=\"STA-SSID\">SSID</label>    <input type=\"text\" name=\"STA-SSID\">    <br>    <label for=\"STA-Password\">Password</label>    <input type=\"text\" name=\"STA-Password\">    <br>    <button type=\"submit\" style=\"column-span: 3;\">Update Network Configuration</button>            </form>    </section><section>    <h1>OSC</h1>    <form method=\"get\" action=\"/OSC\">    <!-- OSC -->    <label for=\"OSC-Remote\">Remote IP</label>    <input type=\"text\" name=\"OSC-Remote\">    <label for=\"OSC-Remote-Port\">Port:</label>    <input type=\"number\" name=\"OSC-Remote-Port\">    <br>    <label for=\"OSC-Local\">Local IP</label>    <input type=\"text\" name=\"OSC-Local\">    <label for=\"OSC-Local-Port\">Port:</label>    <input type=\"number\" name=\"OSC-Local-Port\">    <br>    <button type=\"submit\" style=\"column-span: 3;\">Update OSC Configuration</button>            </form>    </section><section><h1>GPIO</h1>    <form method=\"post\" action=\"/GPIO\"> <table><tr><td>Pin - 1</td><td><select name=\"pin1\"> <option value=\"0\">OFF</option><option value=\"1\">Analog</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin1-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 2</td><td><select name=\"pin2\"> <option value=\"0\">OFF</option><option value=\"1\">Analog</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin2-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 3</td><td><select name=\"pin3\"> <option value=\"0\">OFF</option><option value=\"1\">Analog</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin3-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 4</td><td><select name=\"pin4\"> <option value=\"0\">OFF</option><option value=\"1\">Analog</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin4-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 5</td><td><select name=\"pin5\"> <option value=\"0\">OFF</option><option value=\"1\">Analog</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin5-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 6</td><td><select name=\"pin6\"> <option value=\"0\">OFF</option><option value=\"1\">Analog</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin6-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 7</td><td><select name=\"pin7\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin7-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 8</td><td><select name=\"pin8\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin8-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 9</td><td><select name=\"pin9\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin9-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 10</td><td><select name=\"pin10\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin10-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 11</td><td><select name=\"pin11\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin11-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 12</td><td><select name=\"pin12\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin12-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 13</td><td><select name=\"pin13\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin13-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 14</td><td><select name=\"pin14\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin14-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 15</td><td><select name=\"pin15\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin15-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 16</td><td><select name=\"pin16\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin16-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 17</td><td><select name=\"pin17\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin17-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 18</td><td><select name=\"pin18\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin18-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 19</td><td><select name=\"pin19\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin19-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 20</td><td><select name=\"pin20\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin20-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 21</td><td><select name=\"pin21\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin21-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 22</td><td><select name=\"pin22\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin22-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 23</td><td><select name=\"pin23\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin23-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 24</td><td><select name=\"pin24\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin24-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 25</td><td><select name=\"pin25\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin25-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 26</td><td><select name=\"pin26\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin26-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 27</td><td><select name=\"pin27\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin27-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr><tr><td>Pin - 28</td><td><select name=\"pin28\"> <option value=\"0\">OFF</option><option value=\"2\">Digital</option></select></td><td>    <select name=\"pin28-io\">        <option value=\"0\">Output</option>        <option value=\"1\">Input</option>    </select></td></tr></table><button type=\"submit\">Save </button></form></body>    ";
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t Conf_Reset(httpd_req_t *req)
{
    // ESP_ERROR_CHECK(nvs_save_int("Config", "Network", -1));
    uint32_t mode = AP_MODE_END;
    ESP_ERROR_CHECK(nvs_update(&nvs_global, "net_settings.network_mode", &mode));

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

    const bool isAP = strcmp(mode, "AP") == 0;
    const bool isSTA = strcmp(mode, "STA") == 0;
    uint32_t mode_value;

    if (isAP)
    {
        mode_value = AP;
    }
    else if (isSTA)
    {
        mode_value = STA;
    }
    else
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid mode");
    }
    ESP_ERROR_CHECK(nvs_update(&nvs_global, "net_settings.network_mode", &mode_value));

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

    ESP_ERROR_CHECK(nvs_update(&nvs_global, "net_settings.ap_ssid", AP_SSID));
    ESP_ERROR_CHECK(nvs_update(&nvs_global, "net_settings.ap_password", AP_Password));

    ESP_ERROR_CHECK(nvs_update(&nvs_global, "net_settings.sta_ssid", STA_SSID));
    ESP_ERROR_CHECK(nvs_update(&nvs_global, "net_settings.sta_password", STA_Password));

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

    const size_t qlen = httpd_req_get_url_query_len(req);
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

    const int remote_port = atoi(value);

    // --- Local IP ---
    if (httpd_query_key_value(query, "OSC-Local", value, sizeof(value)) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing OSC-Local");

    char local_ip[64];
    strcpy(local_ip, value);

    // --- Local Port ---
    if (httpd_query_key_value(query, "OSC-Local-Port", value, sizeof(value)) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing OSC-Local-Port");

    const int local_port = atoi(value);

    ESP_ERROR_CHECK(nvs_update(&nvs_global, "osc_settings.remote_ip", &remote_ip));
    ESP_ERROR_CHECK(nvs_update(&nvs_global, "osc_settings.remote_port", &remote_port));

    ESP_ERROR_CHECK(nvs_update(&nvs_global, "osc_settings.local_ip", &local_ip));
    ESP_ERROR_CHECK(nvs_update(&nvs_global, "osc_settings.local_port", &local_port));

    // Respond immediately so browser stops loading
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, "OSC Saved", HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
};

static esp_err_t GPIO_Handler(httpd_req_t *req)
{
    // --- Read POST body ---
    const int total = req->content_len;
    if (total <= 0 || total > 1024)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid body");
    }

    char body[1024];
    const int received = httpd_req_recv(req, body, sizeof(body) - 1);

    if (received <= 0)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read body");
    }

    body[received] = '\0'; // Null‑terminate

    // --- Parse all pins ---
    char value[32];

    for (int i = 1; i <= PINCOUNT; i++)
    {
        char field_mode[16];
        char field_io[16];

        snprintf(field_mode, sizeof(field_mode), "pin%d", i);
        snprintf(field_io, sizeof(field_io), "pin%d-io", i);

        if (httpd_query_key_value(body, field_mode, value, sizeof(value)) == ESP_OK)
        {
            int mode = atoi(value);
            if (mode < GPIO_OFF || mode > GPIO_DIGITAL)
                mode = GPIO_OFF;
            nvs_global.gpio_settings.pin_mode[i - 1] = (GPIO_State)mode;
        }

        if (httpd_query_key_value(body, field_io, value, sizeof(value)) == ESP_OK)
        {
            int io = atoi(value);
            if (io < 0 || io > 1)
                io = 0;
            nvs_global.gpio_settings.pin_io[i - 1] = (GPIO_IO)io;
        }
    }

    esp_err_t err = nvs_save_gpio_pins(nvs_global.gpio_settings.pin_mode, nvs_global.gpio_settings.pin_io);
    if (err != ESP_OK)
    {
        ESP_LOGE("GPIO_Handler", "Failed to persist pin config: %s", esp_err_to_name(err));
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

    for (int i = 1; i <= PINCOUNT; i++)
    {
        // Append JSON entry
        offset += snprintf(json + offset, sizeof(json) - offset,
                           "\"%d\": {\"mode\": %d, \"io\": %d}%s",
                           i, nvs_global.gpio_settings.pin_mode[i - 1], nvs_global.gpio_settings.pin_io[i - 1],
                           (i < PINCOUNT ? "," : ""));
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

    // Build JSON
    offset += snprintf(json + offset, sizeof(json) - offset,
                       "{"
                       "\"mode\":\"%s\","
                       "\"AP_SSID\":\"%s\","
                       "\"AP_Password\":\"%s\","
                       "\"STA_SSID\":\"%s\","
                       "\"STA_Password\":\"%s\""
                       "}",
                       (nvs_global.net_settings.network_mode == AP ? "AP" : "STA"),
                       nvs_global.net_settings.ap_ssid,
                       nvs_global.net_settings.ap_password,
                       nvs_global.net_settings.sta_ssid,
                       nvs_global.net_settings.sta_password);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));

    return ESP_OK;
}

static esp_err_t osc_json_handler(httpd_req_t *req)
{
    char json[256];
    int offset = 0;

    // Build JSON
    offset += snprintf(json + offset, sizeof(json) - offset,
                       "{"
                       "\"remote_ip\":\"%s\","
                       "\"remote_port\":%d,"
                       "\"local_ip\":\"%s\","
                       "\"local_port\":%d"
                       "}",
                       nvs_global.osc_settings.remote_ip,
                       nvs_global.osc_settings.remote_port,
                       nvs_global.osc_settings.local_ip,
                       nvs_global.osc_settings.local_port);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));

    return ESP_OK;
}

static const httpd_uri_t base_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = base_handler,
    .user_ctx = NULL,
};

static const httpd_uri_t reset_uri = {
    .uri = "/reset",
    .method = HTTP_GET,
    .handler = Conf_Reset,
    .user_ctx = NULL,
};

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
    .user_ctx = NULL,
};

static const httpd_uri_t osc_json_uri = {
    .uri = "/osc.json",
    .method = HTTP_GET,
    .handler = osc_json_handler,
    .user_ctx = NULL,
};

static const httpd_uri_t network_json_uri = {
    .uri = "/network.json",
    .method = HTTP_GET,
    .handler = network_json_handler,
    .user_ctx = NULL,
};

// Defines the Full Http server
httpd_handle_t start_webserver()
{

    httpd_handle_t server = NULL;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // config.lru_purge_enable = true;

    if (httpd_start(&server, &config) == ESP_OK)
    {

        ESP_LOGI("HTTP Server", "Server ok, registering the URI handlers...");

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

    ESP_LOGI("HTTP Server", "Error starting server");

    return NULL;
}

// UDP

char *getCurrentIP()
{
    static char ip_str[16];
    esp_netif_ip_info_t ip_info;

    esp_netif_t *netif = NULL;

    switch (nvs_global.net_settings.network_mode)
    {
    case STA:
        netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        break;
    case AP:
        netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
        break;
    case AP_MODE_END:
        break;
    }

    if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK)
    {
        sprintf(ip_str, IPSTR, IP2STR(&ip_info.ip));
        return ip_str;
    }

    return "0.0.0.0";
}

// OSC message server

// Helper: parse channel and validate numeric suffix
static int parseChannelValidated(const char *addr, const char *prefix, const int min_ch, const int max_ch)
{
    const char *p = addr + strlen(prefix);
    if (!p || *p == '\0')
    {
        return -1; // no suffix
    }

    // ensure suffix is numeric (allow multi-digit)
    for (const char *q = p; *q; ++q)
    {
        if (!isdigit((unsigned char)*q))
        {
            return -1;
        }
    }

    const int ch = atoi(p);
    if (ch < min_ch || ch > max_ch)
    {
        return -1;
    }
    return ch;
}

static void ProcessMessage(const OscTimeTag *const oscTimeTag, OscMessage *const oscMessage)
{
    const char *addr = oscMessage->oscAddressPattern;
    if (addr == NULL)
    {
        ESP_LOGW("OSC_Process", "NULL address in message");
        flashLedRed();
        return;
    }

    const size_t route_count = sizeof(ROUTES) / sizeof(ROUTES[0]);

    for (size_t i = 0; i < route_count; ++i)
    {
        const OscRoute *r = &ROUTES[i];

        if (r->has_channel)
        {
            // prefix match: route prefix must match start of addr
            size_t plen = strlen(r->prefix);
            if (strncmp(addr, r->prefix, plen) != 0)
            {
                continue;
            }

            // parse and validate channel (example valid range 1..16; adjust if needed)
            const int channel = parseChannelValidated(addr, r->prefix, 1, PINCOUNT);
            if (channel < 0)
            {
                ESP_LOGW("OSC_Process", "Matched prefix '%s' but invalid channel in '%s'", r->prefix, addr);
                flashLedRed();
                return;
            }

            // call handler with validated channel
            r->handler(oscMessage, channel, &nvs_global);
            return;
        }
        else
        {
            // exact match for non-channel routes
            if (OscAddressMatch(addr, r->prefix))
            {
                r->handler(oscMessage, -1, &nvs_global);
                return;
            }
        }
    }

    // No match → flash red LED
    ESP_LOGW("OSC_Process", "No route for address '%s'", addr);
    flashLedRed();
}

// OSC
void received(const void *const data, const size_t number_of_bytes){
    // Process OSC
    OscPacket oscPacket;
    OscPacketInitialiseFromCharArray(&oscPacket, data, number_of_bytes);
    oscPacket.processMessage = ProcessMessage;
    OscPacketProcessMessages(&oscPacket);
}

static OscError sendOscContents(const void *const oscContents)
{
    OscPacket OscPacket;
    OscError err = OscPacketInitialiseFromContents(&OscPacket, oscContents); 
    if (err != OscErrorNone)
    {
        return err;
    }
    udp_send_osc(OscPacket.contents,OscPacket.size);
    return OscErrorNone;
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
    OscMessageAddString(&oscMessage, FIRMWARE_VERSION);

    // Send the OSC message
    sendOscContents(&oscMessage);
}

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

        const int channel = i - 1;

        adc_oneshot_chan_cfg_t chan_cfg = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };

        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, channel, &chan_cfg));
    }
}

int readDigitalPin(const int pin)
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

float readAnaloguePin(const int pin)
{
    const int channel = pin - 1;

    int raw = 0;
    ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, channel, &raw));

    return (float)raw / 4095.0f;
}

static int last_digital[PINCOUNT] = {0};

void send_digital_inputs(void)
{
    int changed = 0;
    int values[PINCOUNT];

    for (int i = 1; i < PINCOUNT + 1; i++)
    {
        if (nvs_global.gpio_settings.pin_mode[i - 1] == GPIO_DIGITAL)
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
        if (nvs_global.gpio_settings.pin_mode[i - 1] == GPIO_ANALOGUE)
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
        // Used to send only data on configured rate
        vTaskDelay(pdMS_TO_TICKS(nvs_global.gpio_settings.gpio_rate));
    }
}

void app_main(void)
{
    // Initialise Non-Volatile Storage
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Test OSC fail blink - (first call initalises the strip)
    flashLedRed();
    flashLedRed();

    // Loads the Current Network Mode
    uint32_t network_mode_default = AP_MODE_END; // pick your actual desired default
    uint32_t network_mode = 0;
    esp_err_t err = nvs_load_value("net_settings", "network_mode", FIELD_ENUM, &network_mode_default, &network_mode);
    ESP_LOGE("NVS_LOAD", "%s", esp_err_to_name(err));
    if (network_mode < AP || network_mode >= AP_MODE_END)
    {
        init_default_Config(&nvs_global);
        esp_restart();
    }

    // Populates Global nvs
    err = nvs_populate_all(&nvs_global, &NVS_DEFAULTS);
    if (err != ESP_OK)
    {
        ESP_LOGE("NVS_POPULATE", "nvs_populate_all failed: %s", esp_err_to_name(err));
        vTaskDelay(pdTICKS_TO_MS(1000));
        return;
    }

    esp_err_t gpio_err = nvs_load_gpio_pins(nvs_global.gpio_settings.pin_mode, nvs_global.gpio_settings.pin_io, NVS_DEFAULTS.gpio_settings.pin_mode, NVS_DEFAULTS.gpio_settings.pin_io);
    if (gpio_err != ESP_OK && gpio_err != ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGE("NVS_GPIO", "Failed to load pin config: %s", esp_err_to_name(gpio_err));
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_t *ap = NULL;
    esp_netif_t *sta = NULL;

    // Has to be initalised on a switch case due to esp_netif_create_default_wifi_(mode) starting its own threat that can cause problems
    if (network_mode == AP)
    {
        ap = esp_netif_create_default_wifi_ap();
    }
    else
    {
        sta = esp_netif_create_default_wifi_sta();
    }

    const wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register handlers (Network-related)
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    switch (network_mode)
    {
    case AP:
        // --- AP ONLY PATH ---
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_LOGI(TAG_AP, "ESP_WIFI_MODE_AP");

        wifi_config_t cfg = {

        };
        esp_err_t err = wifi_configure_softap(&nvs_global, &cfg);
        ESP_ERROR_CHECK(esp_wifi_start());

        // No event group wait here
        (void)ap; // if unused for now

        break;
    case STA:
        // --- STA PATH ---
        // Create event group only for STA
        s_wifi_event_group = xEventGroupCreate();
        assert(s_wifi_event_group != NULL);

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_LOGI(TAG_STA, "ESP_WIFI_MODE_STA");
        
        wifi_config_t cfg = {

        };
        esp_err_t err =  wifi_configure_station(&nvs_global, &cfg);
        ESP_ERROR_CHECK(esp_wifi_start());

        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

        if (bits & WIFI_CONNECTED_BIT)
        {
            ESP_LOGI(TAG_STA, "connected to ap SSID:%s password:%s", CONFIG_ESP_WIFI_REMOTE_AP_SSID, CONFIG_ESP_WIFI_REMOTE_AP_PASSWORD);
        }
        else if (bits & WIFI_FAIL_BIT)
        {
            ESP_LOGE(TAG_STA, "Failed to connect to SSID:%s, password:%s", CONFIG_ESP_WIFI_REMOTE_AP_SSID, CONFIG_ESP_WIFI_REMOTE_AP_PASSWORD);
            // ESP_ERROR_CHECK(nvs_save_int("Config", "Network", AP));
            uint32_t AP_value = AP;
            ESP_ERROR_CHECK(nvs_update(&nvs_global, "net_settings.network_mode", &AP_value));
            esp_restart();
        }
        else
        {
            ESP_LOGE(TAG_STA, "UNEXPECTED EVENT");
        }
        break;
    }

    // Start HTTP server directly
    httpd_handle_t server = start_webserver();
    (void)server;

    // Spawns the recive Server that handles all remote -> x-osc2 messages + makes socket

    networking_init(&nvs_global.osc_settings,);
    

    // Allows for analogue pin reads
    adc_init();

    // Spawns a task that sends the Current Configured Gpio
    xTaskCreate(gpio_task, "GPIO Task", 8192, NULL, 5, NULL);
}
