#pragma once

#include <stdint.h>
#include "driver/gpio.h"

/**
 * @brief Required pin state for the stepper motor.
 * True = HIGH, False = LOW
 */
typedef struct {
    bool pin1;
    bool pin2;
    bool pin3;
    bool pin4;
} StepperPins_t;

class Stepper {
    public:
        Stepper(gpio_num_t hallPin, bool direction, uint8_t pin1, uint8_t pin2, uint8_t pin3, uint8_t pin4);

        // Set the target step to stepNum, step() will keep stepping until the position is reached
        void setTarget(int stepNum);
        
        // Get the pin values required for the next step, or current step if not moving
        StepperPins_t step();

        // Return if the stepper is currently at position 0, verified with hall sensor
        bool isHome();

        // Return if the stepper is currently at the desired target step / position
        bool isAtTarget();

        // Return if the hall sensor is currently active, useful for initial positioning of the motor on start up
        bool hallActive();

        // Return the current position of the stepper
        int getPosition();

    private:
        // Get the high (true) / low (false) values for each pin for the provided position
        StepperPins_t getPinState(int stepNumber);

        // Check the hall position, so we know when we've hit 0 / home
        bool checkHall();

        gpio_num_t _hallPin;
        bool _direction;
        uint8_t pinMap[4];
        int _currentPosition = 0;
        int _targetPosition = 0;
        bool _hallActive = false;
        bool _canCheckHallState = true;
        int _hallRepeatCount = 0;
        int _stepsSinceHall = 0;
        int _fullRotationSteps = 0;
};