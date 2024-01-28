#include "display.hpp"
#include "calibrate.hpp"
#include "esp_log.h"
#include <chrono>
#include <esp_pthread.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "DISPLAY";
static const long long workerWaitMs = 100;
static const size_t maxQueueLength = 10;

Display::Display(MultiStepper &multiStepper):
    _multiStepper(multiStepper) {
}

Display::~Display() {
    stop();
}

void Display::start() {
    if (_active.load())
        return;

    // Start up the worker
    auto cfg = esp_pthread_get_default_config();
    cfg.thread_name = "Display";
    cfg.stack_size = 1024 * 16;
    cfg.pin_to_core = 1;
    cfg.prio = 6;
    esp_pthread_set_cfg(&cfg);
    _workerThread = std::thread(&Display::worker, this);

    _active = true;
}

void Display::stop() {
    std::lock_guard<std::mutex> lck(_messageQueueLock);

    if (!_active.load())
        return;

    while (_messageQueue.size() > 0)
        _messageQueue.pop();

    _active = false;
    _ready = false;
}

bool Display::enqueueMessage(DisplayMessage_t message) {
    std::lock_guard<std::mutex> lck(_messageQueueLock);
    if (_messageQueue.size() > maxQueueLength) {
        ESP_LOGW(TAG, "Max queue size reached, message rejected");
        return false;
    }
    _messageQueue.push(message);

    return true;
}

void Display::clearQueue() {
    std::lock_guard<std::mutex> lck(_messageQueueLock);
    while (_messageQueue.size() > 0)
        _messageQueue.pop();
}

void Display::worker() {
    // Need to make sure the units are all homed and happy
    initUnits();
    clearQueue();
    _ready = true;

    // Process messages as they come
    while (_active.load()) {
        // Always a little delay so we don't consume all the resources busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(workerWaitMs));

        // Get next message
        DisplayMessage_t message;
        {
            std::lock_guard<std::mutex> lck(_messageQueueLock);
            if (_messageQueue.empty()) {
                continue;
            }
            message = _messageQueue.front();
            _messageQueue.pop();
        }

        // Display the message
        ESP_LOGI(TAG, "Displaying message");
        for (uint8_t i = 0; i < CONFIG_UNITS_COUNT; i++) {
            int position = _unitCalibrations[i].getPositionForCharacter(message.message[i]);
            _multiStepper.setTargetPosition(i, position);
        }
        _multiStepper.moveAllUnits();
        
        // Sleep for the minimum display duration
        ESP_LOGI(TAG, "Message displayed");
        long long delayMs = message.minShowMs - workerWaitMs;
        if (delayMs > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    }
}

void Display::initUnits() {
    ESP_LOGI(TAG, "Homing Units");

    _multiStepper.home();

    ESP_LOGI(TAG, "Calibrating Units");
    Calibrate calibrate(&_multiStepper);
    calibrate.calibrate();
    calibrate.populateCalibrations(_unitCalibrations, CONFIG_UNITS_COUNT);
}
