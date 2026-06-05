/*  WiFi softAP Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include "OscError.h"
#include "OscPacket.h"
#include "OscSlip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"


#include "esp_http_server.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

#include "Osc99.h"
#include "inttypes.h"
#include "string.h"
#include "nvs_config.h"
#include "nvs_flash.h"
#include "nvs.h" 


// #include "driver/gpio.h"




/* UDP socket tests */

#define PORT CONFIG_EXAMPLE_PORT

static const char *TAG = "example";
static int sock = -1;
static struct sockaddr_storage last_client_addr;
static socklen_t last_client_len = 0;

void udp_send_message(const char *msg)
{
    if (sock < 0) {
        ESP_LOGE(TAG, "Socket not initialized");
        return;
    }

    if (last_client_len == 0) {
        ESP_LOGE(TAG, "No client address available");
        return;
    }

    int err = sendto(
        sock,
        msg,
        strlen(msg),
        0,
        (struct sockaddr *)&last_client_addr,
        last_client_len
    );

    if (err < 0) {
        ESP_LOGE(TAG, "Send failed: errno %d", errno);
    } else {
        ESP_LOGI(TAG, "Sent: %s", msg);
    }
}

void udp_send_osc(const char *msg)
{
    if (sock < 0) {
        ESP_LOGE(TAG, "Socket not initialized");
        return;
    }

    if (last_client_len == 0) {
        ESP_LOGE(TAG, "No client address available");
        return;
    }

    int err = sendto(
        sock,
        msg,
        strlen(msg),
        0,
        (struct sockaddr *)&last_client_addr,
        last_client_len
    );

    if (err < 0) {
        ESP_LOGE(TAG, "Send failed: errno %d", errno);
    } else {
        ESP_LOGI(TAG, "Sent: %s", msg);
    }
}



OscSlipDecoder oscSlipDecoder;

/* OSC mock up functions */

void sendOscContents(const void* const oscContents) {
    OscPacket OscPacket;
    if (OscPacketInitialiseFromContents(&OscPacket, oscContents) != OscErrorNone){
        return;
    }

    // encode a slip packet 
    char slipPacket[MAX_OSC_PACKET_SIZE];
    size_t slipPacketSize;
    if (OscSlipEncodePacket(&OscPacket, &slipPacketSize, slipPacket, sizeof(slipPacket))){
        return;
    }

    // send Packet 

    udp_send_message(slipPacket);
}



void sendHelloMessage() {
  OscMessage oscMessage;
  OscMessageInitialise(&oscMessage, "/hello");
  OscMessageAddString(&oscMessage, "Hi!");
  sendOscContents(&oscMessage);
}


static void udp_server_task(void *pvParameters)
{
    char rx_buffer[128];
    char addr_str[128];
    int addr_family = (int)pvParameters;
    int ip_protocol = 0;
    struct sockaddr_in6 dest_addr;

    while (1) {

        if (addr_family == AF_INET) {
            struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
            dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
            dest_addr_ip4->sin_family = AF_INET;
            dest_addr_ip4->sin_port = htons(PORT);
            ip_protocol = IPPROTO_IP;
        } else if (addr_family == AF_INET6) {
            bzero(&dest_addr.sin6_addr.un, sizeof(dest_addr.sin6_addr.un));
            dest_addr.sin6_family = AF_INET6;
            dest_addr.sin6_port = htons(PORT);
            ip_protocol = IPPROTO_IPV6;
        }

        sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket created");

#if defined(CONFIG_LWIP_NETBUF_RECVINFO) && !defined(CONFIG_EXAMPLE_IPV6)
        int enable = 1;
        lwip_setsockopt(sock, IPPROTO_IP, IP_PKTINFO, &enable, sizeof(enable));
#endif

#if defined(CONFIG_EXAMPLE_IPV4) && defined(CONFIG_EXAMPLE_IPV6)
        if (addr_family == AF_INET6) {
            // Note that by default IPV6 binds to both protocols, it is must be disabled
            // if both protocols used at the same time (used in CI)
            int opt = 1;
            setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
            setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));
        }
#endif
        // Set timeout
        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

        int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0) {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        }
        ESP_LOGI(TAG, "Socket bound, port %d", PORT);

        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        socklen_t socklen = sizeof(source_addr);

#if defined(CONFIG_LWIP_NETBUF_RECVINFO) && !defined(CONFIG_EXAMPLE_IPV6)
        struct iovec iov;
        struct msghdr msg;
        struct cmsghdr *cmsgtmp;
        u8_t cmsg_buf[CMSG_SPACE(sizeof(struct in_pktinfo))];

        iov.iov_base = rx_buffer;
        iov.iov_len = sizeof(rx_buffer);
        msg.msg_control = cmsg_buf;
        msg.msg_controllen = sizeof(cmsg_buf);
        msg.msg_flags = 0;
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_name = (struct sockaddr *)&source_addr;
        msg.msg_namelen = socklen;
#endif

        while (1) {
            ESP_LOGI(TAG, "Waiting for data");
#if defined(CONFIG_LWIP_NETBUF_RECVINFO) && !defined(CONFIG_EXAMPLE_IPV6)
            int len = recvmsg(sock, &msg, 0);
#else
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
#endif
            // Error occurred during receiving
            if (len < 0) {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                break;
            }
            // Data received
            else {
                // Get the sender's ip address as string
                if (source_addr.ss_family == PF_INET) {
                    inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
#if defined(CONFIG_LWIP_NETBUF_RECVINFO) && !defined(CONFIG_EXAMPLE_IPV6)
                    for ( cmsgtmp = CMSG_FIRSTHDR(&msg); cmsgtmp != NULL; cmsgtmp = CMSG_NXTHDR(&msg, cmsgtmp) ) {
                        if ( cmsgtmp->cmsg_level == IPPROTO_IP && cmsgtmp->cmsg_type == IP_PKTINFO ) {
                            struct in_pktinfo *pktinfo;
                            pktinfo = (struct in_pktinfo*)CMSG_DATA(cmsgtmp);
                            ESP_LOGI(TAG, "dest ip: %s", inet_ntoa(pktinfo->ipi_addr));
                        }
                    }
#endif
                } else if (source_addr.ss_family == PF_INET6) {
                    inet6_ntoa_r(((struct sockaddr_in6 *)&source_addr)->sin6_addr, addr_str, sizeof(addr_str) - 1);
                }

                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string...
                ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
                ESP_LOGI(TAG, "%s", rx_buffer);

                memcpy(&last_client_addr, &source_addr, sizeof(source_addr));
                last_client_len = socklen;

                                
                int err = 0;
                if (strcmp(rx_buffer,"/ping")==0) {
                    // err = sendto(sock, rx_buffer, len, 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
                    sendHelloMessage();
                }

                
                if (err < 0) {
                    ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                    break;
                }
            }
        }

        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}




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

// static const char *TAG = "wifi softAP";

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
            char buf[50];
            snprintf(buf, sizeof(buf),"Pin %d New state -> %d",i,new_state);
            udp_send_message(buf);
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

    // const char *keys[] = {
    // "pin1","pin2","pin3","pin4","pin5","pin6","pin7","pin8","pin9","pin10","pin11","pin12","pin13",
    // };
    
    // for (int i = 1; i < sizeof(keys)/sizeof(keys[0]);i++) {
    //         nvs_save_int("Global-Config", keys[i],0);
    //         ESP_LOGI(TAG, "New state = %d", 0);
    //     }


    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    printf("Event WIFI_EVENT_AP_STACONNECTED %d\n",WIFI_EVENT_AP_STACONNECTED);
    wifi_init_softap();
    httpd_handle_t server = start_webserver();
    
    #ifdef CONFIG_EXAMPLE_IPV4
        xTaskCreate(udp_server_task, "udp_server", 8192, (void*)AF_INET, 5, NULL);
    #endif
    #ifdef CONFIG_EXAMPLE_IPV6
        xTaskCreate(udp_server_task, "udp_server", 4096, (void*)AF_INET6, 5, NULL);
    #endif

}
