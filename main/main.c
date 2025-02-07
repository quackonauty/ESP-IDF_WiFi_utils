#include <stdio.h>
#include <WiFi_utils.h>
/* -> ONLY REQUIRED IF YOU WANT A DNS ADDRESS TO GET A PERMANENT CERTIFICATE */
#include <mdns.h>
/* <- ONLY REQUIRED IF YOU WANT A DNS ADDRESS TO GET A PERMANENT CERTIFICATE */

/* WIFI STATION MODE */
static wifi_station_handle_t wifi_station;
static const wifi_access_point_config_t wifi_ap_config = {
    .ssid = "YourSSID",
    .wifi_auth_mode = WIFI_AUTH_WPA2_PSK,
    .password = "YourPassword",
    .wifi_sta_max_retry = 5,
};
/* CLIENT HTTPX REQUEST */
extern const char telegramservercert_start[] asm("_binary_telegramservercert_pem_start");
extern const char telegramservercert_end[] asm("_binary_telegramservercert_pem_end");

void rest_xTask(void *pvparameters)
{
    const char send_message_json[] = "{\"chat_id\": <chat_id>, \"text\": \"ESP32 Hello World\"}";
    ESP_ERROR_CHECK(httpx_rest_url_data("http://api.telegram.org/bot<your_API_token>/sendMessage", HTTP_METHOD_GET, telegramservercert_start, send_message_json, strlen(send_message_json), CONTENT_TYPE_JSON));
    vTaskDelete(NULL);
}

/* HTTPX SERVER */
static httpd_handle_t httpd_server;
extern const uint8_t servercert_start[] asm("_binary_servercert_pem_start");
extern const uint8_t servercert_end[] asm("_binary_servercert_pem_end");
extern const uint8_t prvtkey_start[] asm("_binary_prvtkey_pem_start");
extern const uint8_t prvtkey_end[] asm("_binary_prvtkey_pem_end");

static esp_err_t uri_root_handler(httpd_req_t *req)
{
    const char index_html[] =
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "    <title>Hello World</title>\n"
        "</head>\n"
        "<body>\n"
        "    <h1>Hello World!</h1>\n"
        "</body>\n"
        "</html>\n";
    esp_err_t err = httpd_resp_send(req, index_html, HTTPD_RESP_USE_STRLEN);
    if (err != ESP_OK)
    {
        ESP_LOGE(HTTPS_SERVER_TAG, "Failed to send response");
        return err;
    }
    return ESP_OK;
}

static httpd_uri_t uri_root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = uri_root_handler,
    .user_ctx = NULL};

void app_main(void)
{
    /* WIFI STATION MODE */
    ESP_ERROR_CHECK(nvs_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(wifi_station_init(&wifi_station));
    ESP_ERROR_CHECK(wifi_station_config_ap(&wifi_station, wifi_ap_config));
    ESP_ERROR_CHECK(wifi_station_connect_ap());
    EventBits_t bits = xEventGroupWaitBits(wifi_station.wifi_event_group, WIFI_EVENT_GROUP_CONNECTED_BIT | WIFI_EVENT_GROUP_DISCONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    if (bits & WIFI_EVENT_GROUP_CONNECTED_BIT)
    {
        ESP_LOGI("MAIN", "CONENCTED");
        /* -> ONLY REQUIRED IF YOU WANT A DNS ADDRESS TO GET A PERMANENT CERTIFICATE */
        ESP_ERROR_CHECK(mdns_init());
        ESP_ERROR_CHECK(mdns_hostname_set("example32"));
        ESP_ERROR_CHECK(mdns_instance_name_set("ESP32"));
        /* <- ONLY REQUIRED IF YOU WANT A DNS ADDRESS TO GET A PERMANENT CERTIFICATE */
        ESP_ERROR_CHECK(https_server_start(&httpd_server, servercert_start, servercert_end - servercert_start, prvtkey_start, prvtkey_end - prvtkey_start));
        httpd_register_uri_handler(httpd_server, &uri_root);
        xTaskCreate(rest_xTask, "rest_xTask", 1024 * 5, NULL, 5, NULL);
    }
}
