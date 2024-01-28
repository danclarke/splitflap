#include "clock.hpp"
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <esp_pthread.h>
#include "esp_log.h"

static const char* TAG = "CLOCK";

Clock::Clock(Display &display): _display(display) {
    memset(_message.message, 0, sizeof(char)*10);
    _message.minShowMs = 500;
}

void Clock::start() {
    if (_active.load())
        return;

    // Start up the worker
    _active = true;
    auto cfg = esp_pthread_get_default_config();
    cfg.thread_name = "Clock";
    cfg.stack_size = 1024 * 6;
    cfg.pin_to_core = 0;
    cfg.prio = 5;
    esp_pthread_set_cfg(&cfg);
    _workerThread = std::thread(&Clock::worker, this);
}

void Clock::stop() {
    if (!_active.load())
        return;

    _active = false;
}

void Clock::worker() {
    while (_active.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        time_t now = 0;
        struct tm timeInfo;
        time(&now);
        localtime_r(&now, &timeInfo);

        // Only update in 10s blocks
        if (timeInfo.tm_sec % 10 != 0)
            continue;

        if (timeInfo.tm_min % 5 == 0 && timeInfo.tm_sec == 0) {
            showDate(timeInfo);
        } else {
            showTime(timeInfo);
        }
    }
}

void Clock::showTime(struct tm timeInfo) {
    char str[64] = {0};

    snprintf(str, 64, " %02d:%02d:%02d ", timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
    memcpy(_message.message, str, 10);
    _display.clearQueue();
    _display.enqueueMessage(_message);
}

void Clock::showDate(struct tm timeInfo) {
    char str[64] = {0};
    int month = timeInfo.tm_mon + 1;
    int year = timeInfo.tm_year + 1900;

    snprintf(str, 64, "%02d-%02d-%04d", timeInfo.tm_mday, month, year);
    memcpy(_message.message, str, 10);
    _display.clearQueue();
    _display.enqueueMessage(_message);

    // We don't want to block other messages, but we also don't want to show the time for a little bit
    // So just delay the clock thread for a bit
    std::this_thread::sleep_for(std::chrono::seconds(20));
}
