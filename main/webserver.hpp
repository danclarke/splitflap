#pragma once

#include "esp_http_server.h"
#include <atomic>
#include <memory>
#include "displaymanager.hpp"

class WebServer {
    public:
        WebServer(DisplayManager &displayManager);
        
        void start();
        void stop();
    private:
        DisplayManager &_displayManager;
        static std::unique_ptr<char[]> getBody(httpd_req_t *request);
        static esp_err_t responseOk(httpd_req_t *request);
        static esp_err_t responseErr(httpd_req_t *request, httpd_err_code_t errorCode, const char* errorMessage);

        // Methods

        // Status
        esp_err_t getStatus(httpd_req_t *request);
        static esp_err_t getStatusC(httpd_req_t *request) { return ((WebServer*)request->user_ctx)->getStatus(request); }

        // Set display mode
        esp_err_t postMode(httpd_req_t *request);
        static esp_err_t postModeC(httpd_req_t *request) { return ((WebServer*)request->user_ctx)->postMode(request); }

        // Display message on display
        esp_err_t postMessage(httpd_req_t *request);
        static esp_err_t postMessageC(httpd_req_t *request) { return ((WebServer*)request->user_ctx)->postMessage(request); }

        std::atomic_bool _active = false;
        httpd_handle_t _server;
};