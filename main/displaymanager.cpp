#include "displaymanager.hpp"
#include "esp_log.h"
#include <cstring>

static const char* TAG = "DISPLAYMANAGER";

DisplayManager::DisplayManager(Display &display):
    _display(display) {
    _clock = std::unique_ptr<Clock>(new Clock(display));
}

DisplayManager::~DisplayManager() {
    switch (_mode) {
        case DisplayMode::Clock:
            _clock->stop();
            break;
        case DisplayMode::Text:
            break;
    }
}

void DisplayManager::switchMode(DisplayMode mode) {
    std::lock_guard<std::mutex> lck(_accessLock);

    _mode = mode;
    switch (mode) {
        case DisplayMode::Clock:
            _clock->start();
            break;
        
        case DisplayMode::Text:
            _clock->stop();
            break;
    }
}

bool DisplayManager::display(const char* message, int minDisplayMs) {
    std::lock_guard<std::mutex> lck(_accessLock);

    if (_mode != DisplayMode::Text) {
        ESP_LOGW(TAG, "Message requested, but mode is not text");
        return false;
    }

    int messageLen = strlen(message);
    if (messageLen > CONFIG_UNITS_COUNT)
        messageLen = CONFIG_UNITS_COUNT;

    DisplayMessage_t displayMessage;
    displayMessage.minShowMs = minDisplayMs;
    memset(displayMessage.message, 0, sizeof(char)*CONFIG_UNITS_COUNT);
    memcpy(displayMessage.message, message, messageLen);
    return _display.enqueueMessage(displayMessage);
}
