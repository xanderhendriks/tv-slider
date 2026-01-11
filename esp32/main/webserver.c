#include "webserver.h"

#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_spiffs.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <errno.h>
#include <stdio.h>

static const char *TAG = "webserver";
static bool        s_spiffs_mounted;

static esp_err_t ensure_spiffs_mounted(void)
{
    if (s_spiffs_mounted)
    {
        return ESP_OK;
    }

    esp_vfs_spiffs_conf_t conf = {
        .base_path              = "/spiffs",
        .partition_label        = "spiffs",
        .max_files              = 4,
        .format_if_mount_failed = false,
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount SPIFFS (%s)", esp_err_to_name(ret));
        return ret;
    }

    s_spiffs_mounted = true;
    return ESP_OK;
}

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

static esp_err_t send_file(httpd_req_t *req, const char *path, const char *content_type)
{
    if (ensure_spiffs_mounted() != ESP_OK)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "SPIFFS mount failed");
        return ESP_FAIL;
    }

    FILE *file = fopen(path, "r");
    if (!file)
    {
        ESP_LOGW(TAG, "Failed to open %s (%d)", path, errno);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, content_type);
    char buf[512];
    size_t read_bytes = 0;
    while ((read_bytes = fread(buf, 1, sizeof(buf), file)) > 0)
    {
        if (httpd_resp_send_chunk(req, buf, read_bytes) != ESP_OK)
        {
            fclose(file);
            httpd_resp_sendstr_chunk(req, NULL);
            return ESP_FAIL;
        }
    }
    fclose(file);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

static esp_err_t static_get_handler(httpd_req_t *req)
{
    const char *uri = req->uri;
    if (!uri || uri[0] == '\0' || strcmp(uri, "/") == 0)
    {
        return send_file(req, "/spiffs/index.html", "text/html");
    }

    char uri_path[256];
    size_t uri_len = strcspn(uri, "?");
    if (uri_len >= sizeof(uri_path))
    {
        httpd_resp_send_err(req, HTTPD_414_URI_TOO_LONG, "URI too long");
        return ESP_FAIL;
    }
    memcpy(uri_path, uri, uri_len);
    uri_path[uri_len] = '\0';

    char path[256];
    if (snprintf(path, sizeof(path), "/spiffs%s", uri_path) >= (int) sizeof(path))
    {
        httpd_resp_send_err(req, HTTPD_414_URI_TOO_LONG, "URI too long");
        return ESP_FAIL;
    }

    return send_file(req, path, content_type_for_path(path));
}

static esp_err_t status_get_handler(httpd_req_t *req)
{
    const char *resp = "{\"status\":\"ok\"}";
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

    if (ensure_spiffs_mounted() != ESP_OK)
    {
        return;
    }

    ESP_LOGI(TAG, "Starting HTTP server on port %d", config.server_port);
    if (httpd_start(&server, &config) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return;
    }

    httpd_uri_t status_uri = {
        .uri      = "/status",
        .method   = HTTP_GET,
        .handler  = status_get_handler,
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
    httpd_register_uri_handler(server, &ota_uri);
    httpd_register_uri_handler(server, &static_uri);
}
