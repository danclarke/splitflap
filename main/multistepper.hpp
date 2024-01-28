#pragma once

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/gptimer.h"
#include "driver/gpio.h"
#include "stepper.hpp"

typedef struct {
    SemaphoreHandle_t semaphore;
} stepperTimerData_t;

class MultiStepper {
    public:
        MultiStepper(Stepper *steppers, uint8_t numSteppers, gpio_num_t pinEn, gpio_num_t pinLatch, gpio_num_t pinData, gpio_num_t pinClk, uint64_t stepDelayUs);

        ~MultiStepper();

        // Return the number of steppers attached
        int getNumUnits();

        // Set the destination for a specific unit
        void setTargetPosition(uint8_t unitNumber, int position);

        // Get the position of a specific unit
        int getUnitPosition(uint8_t unitNumber);

        // Move all units to their target position
        void moveAllUnits();

        // Home all the steppers. If this isn't called first, the steppers will be auto-homed on the first movement.
        void home();

    private:
        // Set the pins for every stepper
        void rolloutPins();

        // Start outputting pin values to the shift registers
        void startShift();

        // Finish outputting pin values, output the values
        void endShift();

        // Put the single-bit value into the shift registers. True = high, false = low.
        void shiftOut(bool value);

        // Rotate all motors until the hall sensor is active.
        void moveToMagnet();

        // Step all motors until they're at their target position
        void moveToTarget();

        // Turn off all motors
        void zeroMotors();

        // Setup the callback for motor steps
        void setupTimer();

        Stepper *_steppers;
        uint8_t _numSteppers;
        gpio_num_t _pinEn;
        gpio_num_t _pinLatch;
        gpio_num_t _pinData;
        gpio_num_t _pinClk;
        int _speed = 10;
        bool _homed = false;

        // Speed
        stepperTimerData_t _timerData;
        gptimer_handle_t _timer;
        uint64_t _stepDelay;
};
