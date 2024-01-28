#include "flapmdns.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "mdns.h"

static const char *TAG = "MDNS";

void initFlapMdns() {
    // Initialize mDNS service
    esp_err_t err = mdns_init();
    if (err) {
        ESP_LOGE(TAG, "MDNS Init failed: %d\n", err);
        return;
    }

    // Set hostname
    ESP_ERROR_CHECK(mdns_hostname_set(CONFIG_DNS_HOSTNAME));
    ESP_ERROR_CHECK(mdns_instance_name_set(CONFIG_DNS_NAME));

    // Register services
    ESP_ERROR_CHECK(mdns_service_add(CONFIG_DNS_NAME, "_http", "_tcp", 80, NULL, 0));
    mdns_service_instance_name_set("_http", "_tcp", CONFIG_DNS_NAME);

    ESP_LOGI(TAG, "MDNS initialised, services published");
}