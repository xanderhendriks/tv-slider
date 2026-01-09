#include "webserver.h"

#include "esp_http_server.h"
#include "esp_log.h"

static const char *TAG = "webserver";

static const char *INDEX_HTML =
    "<!doctype html>\n"
    "<html lang=\"en\">\n"
    "<head>\n"
    "  <meta charset=\"utf-8\" />\n"
    "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" />\n"
    "  <title>ESP32 Web</title>\n"
    "  <style>\n"
    "    body { font-family: Arial, sans-serif; margin: 2rem; }\n"
    "    .card { max-width: 520px; padding: 1.5rem; border: 1px solid #ddd; border-radius: 12px; }\n"
    "    h1 { margin-top: 0; }\n"
    "    code { background: #f5f5f5; padding: 0.2rem 0.35rem; border-radius: 6px; }\n"
    "  </style>\n"
    "</head>\n"
    "<body>\n"
    "  <div class=\"card\">\n"
    "    <h1>ESP32 Web Server</h1>\n"
    "    <p>If you can see this page, the HTTP server is running.</p>\n"
    "    <p>Try hitting <code>/status</code> for a JSON response.</p>\n"
    "  </div>\n"
    "</body>\n"
    "</html>\n";

static esp_err_t index_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t status_get_handler(httpd_req_t *req)
{
    const char *resp = "{\"status\":\"ok\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

void webserver_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    ESP_LOGI(TAG, "Starting HTTP server on port %d", config.server_port);
    if (httpd_start(&server, &config) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return;
    }

    httpd_uri_t index_uri = {
        .uri      = "/",
        .method   = HTTP_GET,
        .handler  = index_get_handler,
        .user_ctx = NULL,
    };
    httpd_uri_t status_uri = {
        .uri      = "/status",
        .method   = HTTP_GET,
        .handler  = status_get_handler,
        .user_ctx = NULL,
    };

    httpd_register_uri_handler(server, &index_uri);
    httpd_register_uri_handler(server, &status_uri);
}
