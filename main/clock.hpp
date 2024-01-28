#pragma once

#include "Display.hpp"
#include <thread>
#include <atomic>

class Clock {
    public:
        Clock(Display &display);

        void start();
        void stop();

    private:
        void worker();
        void showTime(struct tm timeInfo);
        void showDate(struct tm timeInfo);

        Display &_display;
        std::atomic_bool _active = false;
        std::thread _workerThread;
        DisplayMessage_t _message;
};