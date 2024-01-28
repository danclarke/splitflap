#pragma once

#include "Display.hpp"
#include "clock.hpp"
#include <memory>
#include <mutex>

enum class DisplayMode {
    Clock,
    Text
};

class DisplayManager {
    public:
        DisplayManager(Display &display);
        ~DisplayManager();

        void switchMode(DisplayMode mode);
        bool display(const char* message, int minDisplayMs);

    private:
        Display &_display;
        std::unique_ptr<Clock> _clock;
        std::mutex _accessLock;
        DisplayMode _mode = DisplayMode::Text;
};