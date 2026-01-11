#include "webserver.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_err.h"
#include "esp_flash.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/inet.h"

static const char *TAG = "webserver";

extern const uint8_t assets_index_html_start[] asm("_binary_index_html_start");
extern const uint8_t assets_index_html_end[] asm("_binary_index_html_end");
extern const uint8_t assets_favicon_ico_start[] asm("_binary_favicon_ico_start");
extern const uint8_t assets_favicon_ico_end[] asm("_binary_favicon_ico_end");

typedef struct
{
    const char    *uri;
    const uint8_t *start;
    const uint8_t *end;
} embedded_asset_t;

static const embedded_asset_t s_assets[] = {{"/index.html", assets_index_html_start, assets_index_html_end},
                                            {"/favicon.ico", assets_favicon_ico_start, assets_favicon_ico_end}};

static const char *content_type_for_path(const char *path)
{
    const char *ext = strrchr(path, '.');
    if (!ext)
    {
        return "text/plain";
    }

    if (strcasecmp(ext, ".html") == 0 || strcasecmp(ext, ".htm") == 0)
    {
        return "text/html";
    }
    if (strcasecmp(ext, ".css") == 0)
    {
        return "text/css";
    }
    if (strcasecmp(ext, ".js") == 0)
    {
        return "application/javascript";
    }
    if (strcasecmp(ext, ".json") == 0)
    {
        return "application/json";
    }
    if (strcasecmp(ext, ".png") == 0)
    {
        return "image/png";
    }
    if (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0)
    {
        return "image/jpeg";
    }
    if (strcasecmp(ext, ".svg") == 0)
    {
        return "image/svg+xml";
    }
    if (strcasecmp(ext, ".ico") == 0)
    {
        return "image/x-icon";
    }

    return "application/octet-stream";
}

static const embedded_asset_t *find_asset(const char *uri_path)
{
    for (size_t i = 0; i < (sizeof(s_assets) / sizeof(s_assets[0])); ++i)
    {
        if (strcmp(uri_path, s_assets[i].uri) == 0)
        {
            return &s_assets[i];
        }
    }
    return NULL;
}

static esp_err_t send_asset(httpd_req_t *req, const embedded_asset_t *asset, const char *content_type)
{
    size_t len = asset->end - asset->start;
    httpd_resp_set_type(req, content_type);
    httpd_resp_send(req, (const char *) asset->start, len);
    return ESP_OK;
}

static esp_err_t static_get_handler(httpd_req_t *req)
{
    const char *uri = req->uri;
    if (!uri || uri[0] == '\0' || strcmp(uri, "/") == 0)
    {
        return send_asset(req, &s_assets[0], "text/html");
    }

    if (strcmp(uri, "/index.html") == 0)
    {
        return send_asset(req, &s_assets[0], "text/html");
    }

    char   uri_path[256];
    size_t uri_len = strcspn(uri, "?");
    if (uri_len >= sizeof(uri_path))
    {
        httpd_resp_send_err(req, HTTPD_414_URI_TOO_LONG, "URI too long");
        return ESP_FAIL;
    }
    memcpy(uri_path, uri, uri_len);
    uri_path[uri_len] = '\0';

    const embedded_asset_t *asset = find_asset(uri_path);
    if (!asset)
    {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }

    return send_asset(req, asset, content_type_for_path(uri_path));
}

static esp_err_t status_get_handler(httpd_req_t *req)
{
    const char *resp = "{\"running\":false, \"error\":\"0\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t info_get_handler(httpd_req_t *req)
{
    const esp_app_desc_t *app_desc = esp_app_get_description();
    esp_netif_t          *netif    = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");

    const char *hostname = "";
    if (netif)
    {
        esp_netif_get_hostname(netif, &hostname);
    }

    esp_netif_ip_info_t ip_info = {0};
    if (netif)
    {
        esp_netif_get_ip_info(netif, &ip_info);
    }
    char ip_str[16];
    snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));

    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));
    char mac_str[20];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    char mcu_str[160];
    snprintf(mcu_str, sizeof(mcu_str), "%s (%d %s)", CONFIG_IDF_TARGET, chip_info.cores,
             chip_info.cores == 1 ? "core" : "cores");

    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);
    char flash_str[32];
    snprintf(flash_str, sizeof(flash_str), "%" PRIu32 "MB %s flash", flash_size / (uint32_t) (1024 * 1024),
             (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    uint32_t min_heap = esp_get_minimum_free_heap_size();
    char     heap_str[32];
    snprintf(heap_str, sizeof(heap_str), "%" PRIu32 " bytes", min_heap);

    char resp[512];
    snprintf(resp, sizeof(resp),
             "{\"hostname\":\"%s\",\"ip_address\":\"%s\",\"mac_address\":\"%s\","
             "\"app_version\":\"%s\",\"compile_time\":\"%s %s\",\"idf_version\":\"%s\","
             "\"mcu\":\"%s\",\"flash\":\"%s\",\"min_heap\":\"%s\"}",
             hostname ? hostname : "", ip_str, mac_str, app_desc ? app_desc->version : "",
             app_desc ? app_desc->date : "", app_desc ? app_desc->time : "", app_desc ? app_desc->idf_ver : "", mcu_str,
             flash_str, heap_str);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t ota_post_handler(httpd_req_t *req)
{
    if (req->content_len <= 0)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
        return ESP_FAIL;
    }

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition");
        return ESP_FAIL;
    }

    esp_ota_handle_t ota_handle = 0;
    esp_err_t        err        = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
        return ESP_FAIL;
    }

    char buf[1024];
    int  remaining = req->content_len;
    while (remaining > 0)
    {
        int received = httpd_req_recv(req, buf, remaining > (int) sizeof(buf) ? (int) sizeof(buf) : remaining);
        if (received <= 0)
        {
            esp_ota_end(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA receive failed");
            return ESP_FAIL;
        }
        err = esp_ota_write(ota_handle, buf, received);
        if (err != ESP_OK)
        {
            esp_ota_end(ota_handle);
            ESP_LOGE(TAG, "esp_ota_write failed (%s)", esp_err_to_name(err));
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA write failed");
            return ESP_FAIL;
        }
        remaining -= received;
    }

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_end failed (%s)", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA end failed");
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA set boot failed");
        return ESP_FAIL;
    }

    httpd_resp_sendstr(req, "OK");
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();
    return ESP_OK;
}

void webserver_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "Starting HTTP server on port %d", config.server_port);
    if (httpd_start(&server, &config) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return;
    }

    httpd_uri_t status_uri = {
        .uri      = "/status/get",
        .method   = HTTP_GET,
        .handler  = status_get_handler,
        .user_ctx = NULL,
    };
    httpd_uri_t info_uri = {
        .uri      = "/info/get",
        .method   = HTTP_GET,
        .handler  = info_get_handler,
        .user_ctx = NULL,
    };
    httpd_uri_t ota_uri = {
        .uri      = "/update",
        .method   = HTTP_POST,
        .handler  = ota_post_handler,
        .user_ctx = NULL,
    };
    httpd_uri_t static_uri = {
        .uri      = "/*",
        .method   = HTTP_GET,
        .handler  = static_get_handler,
        .user_ctx = NULL,
    };

    httpd_register_uri_handler(server, &status_uri);
    httpd_register_uri_handler(server, &info_uri);
    httpd_register_uri_handler(server, &ota_uri);
    httpd_register_uri_handler(server, &static_uri);
}
