#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/i2c.h"
#include "wifi.h"
#include "flapmdns.h"
#include "display.hpp"
#include "multistepper.hpp"
#include "calibrate.hpp"
#include "sntp.h"
#include "clock.hpp"
#include "displaymanager.hpp"
#include "webserver.hpp"
#include "config.h"

static const char *TAG = "Main";

#ifdef CONFIG_UNITS_DIRECTION
bool direction = true;
#else
bool direction = false;
#endif

Stepper steppers[] = {
    Stepper(PIN_HALL_1, direction, STEPPER_PIN1, STEPPER_PIN2, STEPPER_PIN3, STEPPER_PIN4),
    Stepper(PIN_HALL_2, direction, STEPPER_PIN1, STEPPER_PIN2, STEPPER_PIN3, STEPPER_PIN4),
    Stepper(PIN_HALL_3, direction, STEPPER_PIN1, STEPPER_PIN2, STEPPER_PIN3, STEPPER_PIN4),
    Stepper(PIN_HALL_4, direction, STEPPER_PIN1, STEPPER_PIN2, STEPPER_PIN3, STEPPER_PIN4),
    Stepper(PIN_HALL_5, direction, STEPPER_PIN1, STEPPER_PIN2, STEPPER_PIN3, STEPPER_PIN4),
    Stepper(PIN_HALL_6, direction, STEPPER_PIN1, STEPPER_PIN2, STEPPER_PIN3, STEPPER_PIN4),
    Stepper(PIN_HALL_7, direction, STEPPER_PIN1, STEPPER_PIN2, STEPPER_PIN3, STEPPER_PIN4),
    Stepper(PIN_HALL_8, direction, STEPPER_PIN1, STEPPER_PIN2, STEPPER_PIN3, STEPPER_PIN4),
    Stepper(PIN_HALL_9, direction, STEPPER_PIN1, STEPPER_PIN2, STEPPER_PIN3, STEPPER_PIN4),
    Stepper(PIN_HALL_10, direction, STEPPER_PIN1, STEPPER_PIN2, STEPPER_PIN3, STEPPER_PIN4),
};
MultiStepper units(steppers, CONFIG_UNITS_COUNT, PIN_EN, PIN_LATCH, PIN_DATA, PIN_CLK, CONFIG_UNITS_STEP_DELAY_US);
Display display(units);
DisplayManager displayManager(display);
WebServer webServer(displayManager);

// Initialise all important shared ESP32 services
static void initServices();

static void setupLed(void);

extern "C" void app_main(void) {
    // Start up core services
    initServices();
    initWifi();
    display.start();

    // Wait on WiFi to complete init
    if (wifiConnected()) {
        initSntp();
        initFlapMdns();
    } else {
        displayManager.display("WIFI FAIL", 5000);
        ESP_LOGE(TAG, "Could not connect to WiFi");
        return;
    }

    // Wait for display to be ready
    while (!display.ready()) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    // Finish up with web server start
    webServer.start();
    displayManager.display("INITIALISE", 500);

    // Nominal blink to show init complete
    bool led = true;
    while (true) {
        gpio_set_level(PIN_LED, led ? 0 : 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        led = !led;
    }
}

static void initServices() {
    ESP_LOGI(TAG, "Init LED");
    setupLed();

    ESP_LOGI(TAG, "Init NVS");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

static void setupLed() {
    gpio_reset_pin(PIN_LED);
    gpio_set_direction(PIN_LED, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(PIN_LED, 1);
}
