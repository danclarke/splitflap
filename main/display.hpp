#pragma once

#include "multistepper.hpp"
#include "calibrate.hpp"
#include "config.h"
#include "letters.hpp"
#include <mutex>
#include <atomic>
#include <thread>
#include <queue>

typedef struct {
    char message[unitLettersCount];
    long long minShowMs;
} DisplayMessage_t;

class Display {
    public:
        Display(MultiStepper &multiStepper);
        ~Display();

        void start();
        void stop();
        bool enqueueMessage(DisplayMessage_t message);
        void clearQueue();
        bool ready() { return _active.load() && _ready.load(); }

    private:
        void worker();
        void initUnits();

        MultiStepper &_multiStepper;
        UnitCalibration _unitCalibrations[CONFIG_UNITS_COUNT];

        std::atomic_bool _active = false;
        std::atomic_bool _ready = false;
        std::thread _workerThread;
        std::mutex _messageQueueLock;
        std::queue<DisplayMessage_t> _messageQueue;
};