#include "webserver.hpp"
#include "esp_check.h"
#include "cJSON.h"

static const char* TAG = "WEBSERVER";
static const int bufferSize = 1024 * 8; // 8KB

typedef struct {
    WebServer *webServer;
    esp_err_t (*handler)(httpd_req_t *r);
} callContext_t;

WebServer::WebServer(DisplayManager &displayManager)
    : _displayManager(displayManager) {

}

void WebServer::start() {
    if (_active.load())
        return;

    // Start up the server
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(httpd_start(&_server, &config));

    // Register methods

    // GET STATUS
    httpd_uri_t getStatus = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = getStatusC,
        .user_ctx = this
    };
    httpd_register_uri_handler(_server, &getStatus);

    // POST MODE
    httpd_uri_t postMode = {
        .uri = "/api/mode",
        .method = HTTP_POST,
        .handler = postModeC,
        .user_ctx = this
    };
    httpd_register_uri_handler(_server, &postMode);

    // POST MESSAGE
    httpd_uri_t postMessage = {
        .uri = "/api/message",
        .method = HTTP_POST,
        .handler = postMessageC,
        .user_ctx = this
    };
    httpd_register_uri_handler(_server, &postMessage);

    _active = true;
    ESP_LOGI(TAG, "Webserver UP");
}

void WebServer::stop() {
    if (!_active.load())
        return;

    httpd_stop(_server);
    ESP_LOGI(TAG, "Webserver DOWN");
}

std::unique_ptr<char[]> WebServer::getBody(httpd_req_t *request) {
    int totalLen = request->content_len;
    if (totalLen > bufferSize - 1) {
        httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Content too long");
        return NULL;
    }
    
    std::unique_ptr<char[]> buffer = std::unique_ptr<char[]>(new char[bufferSize]);
    int currentLen = 0;
    int received = 0;
    while (currentLen < totalLen) {
        received = httpd_req_recv(request, &buffer[currentLen], totalLen);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return NULL;
        }
        currentLen += received;
    }
    buffer[totalLen] = '\0';

    return buffer;
}

esp_err_t WebServer::responseOk(httpd_req_t *request) {
    // Assemble JSON
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "message", "OK");
    const char *responseJson = cJSON_Print(root);

    // Return response
    httpd_resp_set_type(request, "application/json");
    httpd_resp_sendstr(request, responseJson);
    cJSON_Delete(root);

    return ESP_OK;
}

esp_err_t WebServer::responseErr(httpd_req_t *request, httpd_err_code_t errorCode, const char* errorMessage) {
    // Assemble JSON
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "message", errorMessage);
    const char *responseJson = cJSON_Print(root);

    // Return response
    httpd_resp_set_type(request, "application/json");
    httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, responseJson);
    cJSON_Delete(root);

    return ESP_FAIL;
}

esp_err_t WebServer::getStatus(httpd_req_t *request) {
    ESP_LOGI(TAG, "Get Status");

    // Assemble JSON object
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "health", "OK");
    const char *statusJson = cJSON_Print(root);

    // Send response
    httpd_resp_set_type(request, "application/json");
    httpd_resp_sendstr(request, statusJson);
    cJSON_Delete(root);

    return ESP_OK;
}

esp_err_t WebServer::postMode(httpd_req_t *request) {
    ESP_LOGI(TAG, "Setting display mode");

    auto body = getBody(request);
    if (body == NULL)
        return ESP_FAIL;

    cJSON *root = cJSON_Parse(body.get());
    const char* mode = cJSON_GetObjectItem(root, "mode")->valuestring;

    if (strcmp(mode, "TEXT") == 0)
        _displayManager.switchMode(DisplayMode::Text);
    else if (strcmp(mode, "CLOCK") == 0)
        _displayManager.switchMode(DisplayMode::Clock);
    else {
        char errorMessage[500];
        snprintf(errorMessage, 500, "Unsupported mode: %s", mode);
        return responseErr(request, HTTPD_400_BAD_REQUEST, errorMessage);
    }

    cJSON_Delete(root);
    return responseOk(request);
}

esp_err_t WebServer::postMessage(httpd_req_t *request) {
    ESP_LOGI(TAG, "Display Message");

    auto body = getBody(request);
    if (body == NULL)
        return ESP_FAIL;

    cJSON *root = cJSON_Parse(body.get());
    const char* message = cJSON_GetObjectItem(root, "message")->valuestring;
    int minDisplayMs = cJSON_GetObjectItem(root, "minDisplayMs")->valueint;
    bool success = _displayManager.display(message, minDisplayMs);
    cJSON_Delete(root);

    if (!success)
        return responseErr(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not display message, check active mode");

    return responseOk(request);
}