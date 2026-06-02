/*  WiFi softAP Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_http_server.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "Osc99.h"
#include "inttypes.h"
#include "string.h"

OscSlipDecoder oscSlipDecoder;

/* The examples use WiFi configuration that you can set via project configuration menu.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_WIFI_CHANNEL   CONFIG_ESP_WIFI_CHANNEL
#define EXAMPLE_MAX_STA_CONN       CONFIG_ESP_MAX_STA_CONN

#if CONFIG_ESP_GTK_REKEYING_ENABLE
#define EXAMPLE_GTK_REKEY_INTERVAL CONFIG_ESP_GTK_REKEY_INTERVAL
#else
#define EXAMPLE_GTK_REKEY_INTERVAL 0
#endif

typedef enum GPIO_STATE {
	OFF,
	ANALOGUE,
	DIGITAL,
    END,
}GPIO_STATE;

static const char *TAG = "wifi softAP";

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);

    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d, reason=%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    }
}

void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
#ifdef CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
            .authmode = WIFI_AUTH_WPA3_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
#else /* CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT */
            .authmode = WIFI_AUTH_WPA2_PSK,
#endif
            .pmf_cfg = {
                    .required = true,
            },
#ifdef CONFIG_ESP_WIFI_BSS_MAX_IDLE_SUPPORT
            .bss_max_idle_cfg = {
                .period = WIFI_AP_DEFAULT_MAX_IDLE_PERIOD,
                .protected_keep_alive = 1,
            },
#endif
            .gtk_rekey_interval = EXAMPLE_GTK_REKEY_INTERVAL,
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, EXAMPLE_ESP_WIFI_CHANNEL);
}

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

/* An HTTP GET handler */
static esp_err_t base_handler(httpd_req_t *req)
{
    print_all_nvs_entries("Global-Config");
    const char* resp_str = "<head><style>table {display: flex;justify-content: center; /* Centers horizontally */align-items: center; /* Centers vertically */height: 100vh; /* Full viewport height */}   tr,th,td {    border:1px solid black;}</style></head><body><form method=\"get\" action=\"/save\"> <table><tr><td>Pin - 1</td><td><select name=\"pin1\"> <option value=\"0\">OFF</option><option value=\"1\">PWM</option><option value=\"2\">Serial</option></select></td></tr><tr><td>Pin - 2</td><td><select name=\"pin2\"> <option value=\"0\">OFF</option><option value=\"1\">PWM</option><option value=\"2\">Serial</option></select></td></tr><tr><td>Pin - 3</td><td><select name=\"pin3\"> <option value=\"0\">OFF</option><option value=\"1\">PWM</option><option value=\"2\">Serial</option></select></td></tr><tr><td>Pin - 4</td><td><select name=\"pin4\"> <option value=\"0\">OFF</option><option value=\"1\">PWM</option><option value=\"2\">Serial</option></select></td></tr><tr><td>Pin - 5</td><td><select name=\"pin5\"> <option value=\"0\">OFF</option><option value=\"1\">PWM</option><option value=\"2\">Serial</option></select></td></tr><tr><td>Pin - 6</td><td><select name=\"pin6\"> <option value=\"0\">OFF</option><option value=\"1\">PWM</option><option value=\"2\">Serial</option></select></td></tr><tr><td>Pin - 7</td><td><select name=\"pin7\"> <option value=\"0\">OFF</option><option value=\"1\">PWM</option><option value=\"2\">Serial</option></select></td></tr><tr><td>Pin - 8</td><td><select name=\"pin8\"> <option value=\"0\">OFF</option><option value=\"1\">PWM</option><option value=\"2\">Serial</option></select></td></tr><tr><td>Pin - 9</td><td><select name=\"pin9\"> <option value=\"0\">OFF</option><option value=\"1\">PWM</option><option value=\"2\">Serial</option></select></td></tr><tr><td>Pin - 10</td><td><select name=\"pin10\"> <option value=\"0\">OFF</option><option value=\"1\">PWM</option><option value=\"2\">Serial</option></select></td></tr><tr><td>Pin - 11</td><td><select name=\"pin11\"> <option value=\"0\">OFF</option><option value=\"1\">PWM</option><option value=\"2\">Serial</option></select></td></tr><tr><td>Pin - 12</td><td><select name=\"pin12\"> <option value=\"0\">OFF</option><option value=\"1\">PWM</option><option value=\"2\">Serial</option></select></td></tr><tr><td>Pin - 13</td><td><select name=\"pin13\"> <option value=\"0\">OFF</option><option value=\"1\">PWM</option><option value=\"2\">Serial</option></select></td></tr></table><button type=\"submit\">Save </button></form></body>    ";    
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;

}

static esp_err_t save_handler(httpd_req_t *req)
{
    char query[256];
    char value[16];

    size_t qlen = httpd_req_get_url_query_len(req);
    if (qlen == 0 || qlen >= sizeof(query)) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing query");
    }

    // Read query string
    httpd_req_get_url_query_str(req, query, sizeof(query));

    // Expected Keys
    const char *keys[] = {
    "pin1","pin2","pin3","pin4","pin5","pin6","pin7","pin8","pin9","pin10","pin11","pin12","pin13",
    };

    for (int i = 1; i < sizeof(keys)/sizeof(keys[0]);i++){

        // Extract ?Test=123
        if (httpd_query_key_value(query, keys[i], value, sizeof(value)) != ESP_OK) {
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'Test' key");
        }

        int new_state = atoi(value);

        if (new_state < OFF || new_state >= END) {
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid GPIO state");
        }

        int old_state = nvs_load_int("Global-Config", keys[i], -1);
        
        if (old_state == -1){
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Config missing");
        }

        // Only write if changed
        if (new_state != old_state) {
            nvs_save_int("Global-Config", keys[i],new_state);
            ESP_LOGI(TAG, "New state = %d", new_state);
        }

    }
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}


static const httpd_uri_t base_uri= {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = base_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t save_uri= {
    .uri       = "/save",
    .method    = HTTP_GET,
    .handler   = save_handler,
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

    httpd_register_uri_handler(server, &save_uri);

    return server;

  }

  ESP_LOGI(TAG, "Error starting server");

  return NULL;

}



void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");

    // NVS test

    
    

    
    


    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    printf("Event WIFI_EVENT_AP_STACONNECTED %d\n",WIFI_EVENT_AP_STACONNECTED);
    wifi_init_softap();
    httpd_handle_t server = start_webserver();
}
