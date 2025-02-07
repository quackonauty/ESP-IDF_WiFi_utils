#pragma once

#include <esp_log.h>

/* NVS */
#include <nvs_flash.h>
#define NVS_TAG "NVS"

esp_err_t nvs_init(void);

/* WIFI STATION MODE */
#include <string.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_wifi.h>

#define WIFI_STATION_TAG "WIFI STATION MODE"
#define WIFI_EVENT_GROUP_CONNECTED_BIT BIT0
#define WIFI_EVENT_GROUP_DISCONNECTED_BIT BIT1
#define WIFI_EVENT_GROUP_CONNECTING_BIT BIT2
#define WIFI_EVENT_GROUP_DISCONNECTING_BIT BIT3

typedef struct
{
    esp_netif_t *wifi_sta_netif;
    EventGroupHandle_t wifi_event_group;
    esp_event_handler_instance_t ip_event_handler;
    esp_event_handler_instance_t wifi_event_handler;
    uint8_t wifi_sta_max_retry;
    uint8_t wifi_sta_retry_count;
    char ip[40];
} wifi_station_handle_t;

typedef struct
{
    char ssid[32];
    wifi_auth_mode_t wifi_auth_mode;
    char password[64];
    uint8_t wifi_sta_max_retry;
} wifi_access_point_config_t;

void wifi_event_handler_cb(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void ip_event_handler_cb(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
esp_err_t wifi_station_init(wifi_station_handle_t *wifi_station);
esp_err_t wifi_station_config_ap(wifi_station_handle_t *wifi_station, wifi_access_point_config_t wifi_ap_config);
esp_err_t wifi_station_connect_ap(void);
esp_err_t wifi_station_disconnect_ap(wifi_station_handle_t *wifi_station);
esp_err_t wifi_station_deinit(wifi_station_handle_t *wifi_station);

/* CLIENT HTTPX REQUEST */
#include <esp_http_client.h>

#define HTTPX_CLIENT_TAG "HTTPX CLIENT"

typedef enum
{
    CONTENT_TYPE_JSON,
    CONTENT_TYPE_FORM_URLENCODED,
    CONTENT_TYPE_TEXT_PLAIN,
    CONTENT_TYPE_OCTET_STREAM
} content_type_t;

typedef struct
{
    char *buffer;
    size_t length;
} httpx_client_response_t;

esp_err_t httpx_rest_url_data(char *url, esp_http_client_method_t method, const char *cert_pem, const void *send_data, size_t data_size, content_type_t content_type);
#define httpx_rest_url(url, method, cert_pem) httpx_rest_url_data(url, method, cert_pem, NULL, 0, 0)

/* HTTPS SERVER */
#include <esp_http_server.h>
#include <esp_https_server.h>

#define SERVER_TAG "SERVER"
#define HTTP_SERVER_TAG "HTTP SERVER"
#define HTTPS_SERVER_TAG "HTTPS SERVER"

esp_err_t http_server_start(httpd_handle_t *httpd_server);
esp_err_t http_server_stop(httpd_handle_t httpd_server);
esp_err_t https_server_start(httpd_handle_t *httpd_server, const uint8_t *servercert, size_t servercert_len, const uint8_t *prvtkey, size_t prvtkey_len);
esp_err_t https_server_stop(httpd_handle_t httpd_server);