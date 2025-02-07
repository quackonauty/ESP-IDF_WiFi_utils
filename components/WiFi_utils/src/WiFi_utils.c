#include "WiFi_utils.h"

/* NVS */
esp_err_t nvs_init(void)
{
    ESP_LOGI(NVS_TAG, "Initializing NVS...");
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGI(NVS_TAG, "NVS requires reformatting");
        if ((err = nvs_flash_erase()) != ESP_OK)
        {
            ESP_LOGE(NVS_TAG, "NVS erase failed");
            return err;
        }
        ESP_LOGI(NVS_TAG, "Reinitializing NVS post-format");
        err = nvs_flash_init();
    }
    if (err != ESP_OK)
    {
        ESP_LOGE(NVS_TAG, "NVS initialization failed");
        return err;
    }
    ESP_LOGI(NVS_TAG, "NVS initialized successfully");

    return err;
}

/* WIFI STATION MODE */
void wifi_event_handler_cb(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    wifi_station_handle_t *wifi_station = (wifi_station_handle_t *)arg;
    switch (event_id)
    {
    case WIFI_EVENT_WIFI_READY:
        ESP_LOGI(WIFI_STATION_TAG, "Wi-Fi ready");
        break;
    case WIFI_EVENT_SCAN_DONE:
        ESP_LOGI(WIFI_STATION_TAG, "Scan done");
        break;
    case WIFI_EVENT_STA_START:
        ESP_LOGI(WIFI_STATION_TAG, "Wi-Fi in station mode started successfully");
        ESP_LOGI(WIFI_STATION_TAG, "Connecting Wi-Fi in station mode to the access point...");
        xEventGroupSetBits(wifi_station->wifi_event_group, WIFI_EVENT_GROUP_CONNECTING_BIT);
        esp_wifi_connect();
        break;
    case WIFI_EVENT_STA_STOP:
        ESP_LOGI(WIFI_STATION_TAG, "Wi-Fi in station mode stopped successfully");
        break;
    case WIFI_EVENT_STA_CONNECTED:
        ESP_LOGI(WIFI_STATION_TAG, "Wi-Fi in station mode connected to the access point successfully");
        ESP_LOGI(WIFI_STATION_TAG, "Getting an IP address from the access point...");
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        strcpy(wifi_station->ip, "0.0.0.0");
        if (xEventGroupGetBits(wifi_station->wifi_event_group) & WIFI_EVENT_GROUP_DISCONNECTING_BIT)
        {
            xEventGroupSetBits(wifi_station->wifi_event_group, WIFI_EVENT_GROUP_DISCONNECTED_BIT);
            ESP_LOGI(WIFI_STATION_TAG, "Wi-Fi in station mode disconnected from access point successfully");
            ESP_LOGI(WIFI_STATION_TAG, "Stopping Wi-Fi in station mode...");
            ESP_ERROR_CHECK(esp_wifi_stop());
        }
        else
        {
            ESP_LOGI(WIFI_STATION_TAG, "Wi-Fi in station mode disconnected from access point");
            if (wifi_station->wifi_sta_retry_count < wifi_station->wifi_sta_max_retry)
            {
                xEventGroupSetBits(wifi_station->wifi_event_group, WIFI_EVENT_GROUP_CONNECTING_BIT);
                ESP_LOGI(WIFI_STATION_TAG, "Retrying connection to access point");
                esp_wifi_connect();
                wifi_station->wifi_sta_retry_count++;
            }
            else
            {
                xEventGroupSetBits(wifi_station->wifi_event_group, WIFI_EVENT_GROUP_DISCONNECTED_BIT);
                ESP_LOGE(WIFI_STATION_TAG, "Failed to reconnect to access point");
                wifi_station->wifi_sta_retry_count = 0;
                ESP_LOGI(WIFI_STATION_TAG, "Stopping Wi-Fi in station mode...");
                ESP_ERROR_CHECK(esp_wifi_stop());
            }
        }
        break;
    case (WIFI_EVENT_STA_AUTHMODE_CHANGE):
        ESP_LOGI(WIFI_STATION_TAG, "Wi-Fi in STA authmode changed");
        break;
    case WIFI_EVENT_HOME_CHANNEL_CHANGE:
        ESP_LOGI(WIFI_STATION_TAG, "Wi-Fi home channel changed");
        break;
    case WIFI_EVENT_STA_BEACON_TIMEOUT:
        ESP_LOGI(WIFI_STATION_TAG, "Wi-Fi beacon timeout");
        break;
    default:
        ESP_LOGW(WIFI_STATION_TAG, "Unhandled Wi-Fi event, ID: %" PRId32, event_id);
        break;
    }
}

void ip_event_handler_cb(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    wifi_station_handle_t *wifi_station = (wifi_station_handle_t *)arg;
    switch (event_id)
    {
    case IP_EVENT_STA_GOT_IP:
        wifi_station->wifi_sta_retry_count = 0;
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        esp_ip4addr_ntoa(&event->ip_info.ip, wifi_station->ip, sizeof(wifi_station->ip));
        xEventGroupSetBits(wifi_station->wifi_event_group, WIFI_EVENT_GROUP_CONNECTED_BIT);
        ESP_LOGI(WIFI_STATION_TAG, "IPv4 address provided: %s", wifi_station->ip);
        break;
    case IP_EVENT_STA_LOST_IP:
        memset(wifi_station->ip, 0, sizeof(wifi_station->ip));
        ESP_LOGI(WIFI_STATION_TAG, "Lost IP");
        break;
    default:
        ESP_LOGW(WIFI_STATION_TAG, "Unhandled IP event, ID: %" PRId32, event_id);
        break;
    }
}

esp_err_t wifi_station_init(wifi_station_handle_t *wifi_station)
{
    ESP_LOGI(WIFI_STATION_TAG, "Initializing Wi-Fi in station mode...");
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(WIFI_STATION_TAG, "Failed to initialize TCP/IP stack");
        return err;
    }
    wifi_station->wifi_sta_netif = esp_netif_create_default_wifi_sta();
    wifi_init_config_t wifi_sta_config = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&wifi_sta_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(WIFI_STATION_TAG, "Failed to initialize Wi-Fi");
        return err;
    }
    wifi_station->wifi_event_group = xEventGroupCreate();
    err = esp_wifi_set_default_wifi_sta_handlers();
    if (err != ESP_OK)
    {
        ESP_LOGE(WIFI_STATION_TAG, "Failed to set Wi-Fi handlers");
        return err;
    }
    err = esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &ip_event_handler_cb, wifi_station, &(wifi_station->ip_event_handler));
    if (err != ESP_OK)
    {
        ESP_LOGE(WIFI_STATION_TAG, "Failed to register IP event handler");
        return err;
    }
    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler_cb, wifi_station, &(wifi_station->wifi_event_handler));
    if (err != ESP_OK)
    {
        ESP_LOGE(WIFI_STATION_TAG, "Failed to register Wi-Fi event handler");
        return err;
    }
    // err = esp_wifi_set_ps(WIFI_PS_NONE);
    // if (err != ESP_OK)
    // {
    //     ESP_LOGE(WIFI_STATION_TAG, "Failed to disable Wi-Fi power save mode");
    //     return err;
    // }
    // err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    // if (err != ESP_OK)
    // {
    //     ESP_LOGE(WIFI_STATION_TAG, "Failed to set Wi-Fi storage to RAM");
    //     return err;
    // }
    // ESP_LOGI(WIFI_STATION_TAG, "Wi-Fi storage set to RAM");
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK)
    {
        ESP_LOGE(WIFI_STATION_TAG, "Failed to set Wi-Fi mode to station");
        return err;
    }
    ESP_LOGI(WIFI_STATION_TAG, "Wi-Fi in station mode initialized successfully");

    return err;
}

esp_err_t wifi_station_config_ap(wifi_station_handle_t *wifi_station, wifi_access_point_config_t wifi_ap_config)
{
    wifi_config_t wifi_config_ap = {
        .sta = {
            .threshold.authmode = wifi_ap_config.wifi_auth_mode,
        }};
    strncpy((char *)wifi_config_ap.sta.ssid, wifi_ap_config.ssid, sizeof(wifi_config_ap.sta.ssid));
    strncpy((char *)wifi_config_ap.sta.password, wifi_ap_config.password, sizeof(wifi_config_ap.sta.password));
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config_ap);
    if (err != ESP_OK)
    {
        ESP_LOGE(WIFI_STATION_TAG, "Failed to configurate Wi-Fi access point information");
        return err;
    }
    wifi_station->wifi_sta_max_retry = wifi_ap_config.wifi_sta_max_retry;
    wifi_station->wifi_sta_retry_count = 0;
    ESP_LOGI(WIFI_STATION_TAG, "Wi-Fi access point information configured successfully");

    return err;
}

esp_err_t wifi_station_connect_ap(void)
{
    ESP_LOGI(WIFI_STATION_TAG, "Starting Wi-Fi in station mode...");
    esp_err_t err = esp_wifi_start();
    if (err != ESP_OK)
    {
        ESP_LOGE(WIFI_STATION_TAG, "Failed to start Wi-Fi in station mode");
        return err;
    }

    return err;
}

esp_err_t wifi_station_disconnect_ap(wifi_station_handle_t *wifi_station)
{
    ESP_LOGI(WIFI_STATION_TAG, "Disconnecting Wi-Fi in station mode from the access point...");
    xEventGroupSetBits(wifi_station->wifi_event_group, WIFI_EVENT_GROUP_DISCONNECTING_BIT);
    esp_err_t err = esp_wifi_disconnect();
    if (err != ESP_OK)
    {
        ESP_LOGE(WIFI_STATION_TAG, "Failed to disconnect Wi-Fi in station mode from the access point");
        return err;
    }

    return err;
}

esp_err_t wifi_station_deinit(wifi_station_handle_t *wifi_station)
{
    ESP_LOGI(WIFI_STATION_TAG, "Deinitializing Wi-Fi in station mode");
    esp_err_t err = esp_wifi_deinit();
    if (err != ESP_OK)
    {
        ESP_LOGE(WIFI_STATION_TAG, "Deinitializing Wi-Fi failed");
        return err;
    }
    err = esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, wifi_station->ip_event_handler);
    if (err != ESP_OK)
    {
        ESP_LOGE(WIFI_STATION_TAG, "Unregistering IP event handler failed");
        return err;
    }
    err = esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_station->wifi_event_handler);
    if (err != ESP_OK)
    {
        ESP_LOGE(WIFI_STATION_TAG, "Unregistering Wi-Fi event handler failed");
        return err;
    }
    err = esp_wifi_clear_default_wifi_driver_and_handlers(wifi_station->wifi_sta_netif);
    if (err != ESP_OK)
    {
        ESP_LOGE(WIFI_STATION_TAG, "Clearing Wi-Fi driver and handlers failed");
        return err;
    }
    if (wifi_station->wifi_event_group)
    {
        vEventGroupDelete(wifi_station->wifi_event_group);
    }
    esp_netif_destroy(wifi_station->wifi_sta_netif);
    ESP_LOGI(WIFI_STATION_TAG, "Wi-Fi deinitialized successfully");

    return err;
}

/* CLIENT HTTPX REQUEST */
static const char *get_client_content_type(content_type_t type)
{
    switch (type)
    {
    case CONTENT_TYPE_JSON:
        return "application/json";
    case CONTENT_TYPE_FORM_URLENCODED:
        return "application/x-www-form-urlencoded";
    case CONTENT_TYPE_TEXT_PLAIN:
        return "text/plain";
    case CONTENT_TYPE_OCTET_STREAM:
        return "application/octet-stream";
    default:
        return "application/octet-stream";
    }
}

static esp_err_t http_response_init(httpx_client_response_t *response)
{
    response->buffer = NULL;
    response->length = 0;
    return ESP_OK;
}

static void http_response_clear(httpx_client_response_t *response)
{
    free(response->buffer);
    response->buffer = NULL;
    response->length = 0;
}

static esp_err_t httpx_event_handler(esp_http_client_event_t *evt)
{
    httpx_client_response_t *response = (httpx_client_response_t *)evt->user_data;
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(HTTPX_CLIENT_TAG, "HTTP EVENT ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(HTTPX_CLIENT_TAG, "HTTP EVENT ON CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(HTTPX_CLIENT_TAG, "HTTP EVENT HEADER SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGI(HTTPX_CLIENT_TAG, "HTTP EVENT ON HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGI(HTTPX_CLIENT_TAG, "HTTP EVENT ON DATA");
        if (!evt->data || evt->data_len == 0)
            break;

        size_t new_length = response->length + evt->data_len;
        char *new_buffer = realloc(response->buffer, new_length + 1);

        if (!new_buffer)
        {
            ESP_LOGE(HTTPX_CLIENT_TAG, "Failed to storage response: %zu bytes", new_length + 1);
            http_response_clear(response);
            return ESP_FAIL;
        }

        response->buffer = new_buffer;
        memcpy(response->buffer + response->length, evt->data, evt->data_len);
        response->length = new_length;
        response->buffer[response->length] = '\0';
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI(HTTPX_CLIENT_TAG, "HTTP EVENT ON FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        http_response_clear(response);
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGD(HTTPX_CLIENT_TAG, "HTTP EVENT REDIRECT");
        esp_http_client_set_header(evt->client, "From", "user@example.com");
        esp_http_client_set_header(evt->client, "Accept", "text/html");
        esp_http_client_set_redirection(evt->client);
        break;
    default:
        ESP_LOGW(HTTPX_CLIENT_TAG, "Unknown event %d in http_event_handler()", evt->event_id);
        break;
    }

    return ESP_OK;
}

esp_err_t httpx_rest_url_data(char *url, esp_http_client_method_t method, const char *cert_pem, const void *send_data, size_t data_size, content_type_t content_type)
{
    httpx_client_response_t response = {0};

    esp_http_client_config_t config = {
        .url = url,
        .method = method,
        .event_handler = httpx_event_handler,
        .user_data = &response,
        .disable_auto_redirect = true,
        .timeout_ms = 10000,
        .transport_type = cert_pem ? HTTP_TRANSPORT_OVER_SSL : HTTP_TRANSPORT_UNKNOWN,
        .cert_pem = cert_pem};

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
    {
        ESP_LOGE(HTTPX_CLIENT_TAG, "Failed to initialize client");
        return ESP_FAIL;
    }

    if (send_data && data_size > 0)
    {
        esp_err_t err = esp_http_client_set_post_field(client, send_data, data_size);
        if (err != ESP_OK)
        {
            ESP_LOGE(HTTPX_CLIENT_TAG, "Failed to set body");
            esp_http_client_cleanup(client);

            return err;
        }
        const char *type_header = get_client_content_type(content_type);
        esp_http_client_set_header(client, "Content-Type", type_header);
        ESP_LOGI(HTTPX_CLIENT_TAG, "Sending %zu bytes as %s", data_size, type_header);
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        ESP_LOGI(HTTPX_CLIENT_TAG, "Status: %d (%" PRId64 " bytes)", esp_http_client_get_status_code(client), esp_http_client_get_content_length(client));
        ESP_LOGI(HTTPX_CLIENT_TAG, "Response (%zu bytes):\n%s", response.length, response.buffer);
    }
    else
    {
        ESP_LOGE(HTTPX_CLIENT_TAG, "Failed to send request");
    }

    esp_http_client_cleanup(client);
    http_response_clear(&response);

    return err;
}

/* HTTPS SERVER */
typedef struct
{
    char ip[INET6_ADDRSTRLEN];
    char port[6];
} httpd_ssl_client_info_t;

static void get_client_address(int sockfd, httpd_ssl_client_info_t *client_info)
{
    struct sockaddr_storage peer_addr;
    socklen_t addr_len = sizeof(peer_addr);
    memset(client_info, 0, sizeof(*client_info));

    if (getpeername(sockfd, (struct sockaddr *)&peer_addr, &addr_len) != 0)
    {
        strlcpy(client_info->ip, "Error", sizeof(client_info->ip));
        strlcpy(client_info->port, "Error", sizeof(client_info->port));
        return;
    }

    if (peer_addr.ss_family == AF_INET)
    {
        struct sockaddr_in *client_addr = (struct sockaddr_in *)&peer_addr;
        inet_ntop(AF_INET, &client_addr->sin_addr, client_info->ip, INET_ADDRSTRLEN);
        snprintf(client_info->port, sizeof(client_info->port), "%d", ntohs(client_addr->sin_port));
    }
    else if (peer_addr.ss_family == AF_INET6)
    {
        struct sockaddr_in6 *client_addr = (struct sockaddr_in6 *)&peer_addr;
        inet_ntop(AF_INET6, &client_addr->sin6_addr, client_info->ip, INET6_ADDRSTRLEN);
        snprintf(client_info->port, sizeof(client_info->port), "%d", ntohs(client_addr->sin6_port));
    }
    else
    {
        strlcpy(client_info->ip, "Unsupported family", sizeof(client_info->ip));
        strlcpy(client_info->port, "N/A", sizeof(client_info->port));
    }
}

static esp_err_t httpx_open_handler(httpd_handle_t hd, int sockfd)
{
    httpd_ssl_client_info_t client_info;
    get_client_address(sockfd, &client_info);

    ESP_LOGI(HTTP_SERVER_TAG, "New client connected: socket=%d, IP=%s, port=%s", sockfd, client_info.ip, client_info.port);

    return ESP_OK;
}

static void httpx_close_handler(httpd_handle_t hd, int sockfd)
{
    httpd_ssl_client_info_t client_info;
    get_client_address(sockfd, &client_info);

    ESP_LOGI(HTTP_SERVER_TAG, "Client disconnected: socket=%d, IP=%s, port=%s", sockfd, client_info.ip, client_info.port);
}

esp_err_t http_server_start(httpd_handle_t *httpd_server)
{
    ESP_LOGI(HTTP_SERVER_TAG, "Starting server...");
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.open_fn = httpx_open_handler;
    config.close_fn = httpx_close_handler;
    esp_err_t err = httpd_start(httpd_server, &config);
    if (err != ESP_OK)
    {
        ESP_LOGE(HTTP_SERVER_TAG, "Failed to start server");
        return err;
    }
    ESP_LOGI(HTTP_SERVER_TAG, "Server started successfully");

    return err;
}

esp_err_t http_server_stop(httpd_handle_t httpd_server)
{
    if (httpd_server)
    {
        esp_err_t err = httpd_stop(httpd_server);
        if (err != ESP_OK)
        {
            ESP_LOGI(HTTP_SERVER_TAG, "Failed to stop server");
            return err;
        }
        ESP_LOGI(HTTP_SERVER_TAG, "HTTP server stoped successfully");
        return err;
    }
    ESP_LOGI(HTTPS_SERVER_TAG, "HTTP server not found");

    return ESP_ERR_NOT_FOUND;
}

esp_err_t https_server_start(httpd_handle_t *httpd_server, const uint8_t *servercert, size_t servercert_len, const uint8_t *prvtkey, size_t prvtkey_len)
{
    ESP_LOGI(HTTPS_SERVER_TAG, "Starting server...");
    httpd_ssl_config_t config = HTTPD_SSL_CONFIG_DEFAULT();
    config.httpd.open_fn = httpx_open_handler;
    config.httpd.close_fn = httpx_close_handler;
    config.servercert = servercert;
    config.servercert_len = servercert_len;
    config.prvtkey_pem = prvtkey;
    config.prvtkey_len = prvtkey_len;
    esp_err_t err = httpd_ssl_start(httpd_server, &config);
    if (err != ESP_OK)
    {
        ESP_LOGE(HTTPS_SERVER_TAG, "Failed to start server");
        return err;
    }
    ESP_LOGI(HTTPS_SERVER_TAG, "Server started successfully");

    return err;
}

esp_err_t https_server_stop(httpd_handle_t httpd_server)
{
    if (httpd_server)
    {
        esp_err_t err = httpd_ssl_stop(httpd_server);
        if (err != ESP_OK)
        {
            ESP_LOGI(HTTPS_SERVER_TAG, "Failed to stop server");
            return err;
        }
        ESP_LOGI(HTTPS_SERVER_TAG, "Server stopped successfully");

        return err;
    }
    ESP_LOGI(HTTPS_SERVER_TAG, "Server not found");

    return ESP_ERR_NOT_FOUND;
}
