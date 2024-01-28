#include "stepper.hpp"
#include <stdlib.h>
#include "esp_log.h"
#include "esp_check.h"

static const char* TAG = "STEPPER";

static const StepperPins_t empty = {
    .pin1 = false,
    .pin2 = false,
    .pin3 = false,
    .pin4 = false
};

Stepper::Stepper(gpio_num_t hallPin, bool direction, uint8_t pin1, uint8_t pin2, uint8_t pin3, uint8_t pin4)
: _hallPin(hallPin), _direction(direction) {
    pinMap[0] = pin1;
    pinMap[1] = pin2;
    pinMap[2] = pin3;
    pinMap[3] = pin4;

    gpio_reset_pin(hallPin);
    ESP_ERROR_CHECK(gpio_set_direction(hallPin, GPIO_MODE_INPUT));
}

void Stepper::setTarget(int stepNum) {
    // If we've counted the number of steps for a full rotation
    // We can 'adjust' the stepNum if it's over this count
    // This is especially useful for steps that are close to the home position
    /*if (_fullRotationSteps > 0) {
        while (stepNum > _fullRotationSteps) {
            stepNum -= _fullRotationSteps;
        }
    }*/

    // Set the target position
    _targetPosition = stepNum;
    _hallRepeatCount = 0;
}

bool Stepper::isHome() {
    checkHall();

    return _targetPosition == 0 && _hallActive;
}

bool Stepper::hallActive() {
    return _hallActive;
}

bool Stepper::isAtTarget() {
    return _targetPosition == _currentPosition;
}

int Stepper::getPosition() {
    return _currentPosition;
}

StepperPins_t Stepper::step() {
    // Already in position, turn off motor and leave as-is
    if (_targetPosition == _currentPosition)
        return empty;

    // Move to next step
    ++_currentPosition;
    ++_stepsSinceHall;

    // Set if we've moved into position 0
    checkHall();

    // Return the required power for the stepper
    return getPinState(_currentPosition % 4);
}

bool Stepper::checkHall() {
    bool active = gpio_get_level(_hallPin) == 0;
    
    // Not interested if not active, other than to reset the hall check
    if (!active) {
        if (!_canCheckHallState) {
            _canCheckHallState = true;
        }

        _hallActive = false;
        return false;
    }

    // We're within the hall area, to allow for steps near the magnet we just
    // stop checking until the magnet disappears
    if (!_canCheckHallState) {
        return _hallActive;
    }

    // We're at home

    // If the target is greater than the number of steps, we'll need to adjust the target down
    // If the target is less than the number of steps, it's accurate and we will continue moving to it as-is
    // This solves 2 problems:
    // 1. The stepper gear ratio doesn't evenly divide into 360 degrees, so we'll always have drift
    // 2. Can put flap positions on a linear line, and not worry about the number of steps for 360 degrees, or offsets
    if (_targetPosition >= _currentPosition)
        _targetPosition -= _currentPosition;

    _hallActive = true;
    _currentPosition = 0;
    _canCheckHallState = false;
    _fullRotationSteps = _stepsSinceHall;
    _stepsSinceHall = 0;
    ++_hallRepeatCount;
    ESP_LOGI(TAG, "Hall %d is at home", _hallPin);

    // Make sure we're not just rotating forever
    if (_hallRepeatCount > 3) {
        ESP_LOGE(TAG, "Stepper rotated 3 times, moving to home instead");
        setTarget(0);
    }

    return _hallActive;
}

StepperPins_t Stepper::getPinState(int stepNumber) {
    bool outputPins[4];

    // If we need to go backwards, invert the step number
    if (_direction == false)
        stepNumber = 3 - stepNumber;

    // TODO: Since it's just 4 states, is it worth pre-calculating and just returning 1 of 4 precalc values?
    switch (stepNumber) {
        case 0: // 1010
            outputPins[pinMap[0]] = true;
            outputPins[pinMap[1]] = false;
            outputPins[pinMap[2]] = true;
            outputPins[pinMap[3]] = false;
            break;

        case 1: // 0110
            outputPins[pinMap[0]] = false;
            outputPins[pinMap[1]] = true;
            outputPins[pinMap[2]] = true;
            outputPins[pinMap[3]] = false;
            break;

        case 2: // 0101
            outputPins[pinMap[0]] = false;
            outputPins[pinMap[1]] = true;
            outputPins[pinMap[2]] = false;
            outputPins[pinMap[3]] = true;
            break;

        case 3: // 1001
            outputPins[pinMap[0]] = true;
            outputPins[pinMap[1]] = false;
            outputPins[pinMap[2]] = false;
            outputPins[pinMap[3]] = true;
            break;

        default:
            // Invalid step, failsafe disable the motor
            ESP_LOGE(TAG, "Invalid step number %d", stepNumber);
            outputPins[0] = false;
            outputPins[1] = false;
            outputPins[2] = false;
            outputPins[3] = false;
    }

    return StepperPins_t {
        .pin1 = outputPins[0],
        .pin2 = outputPins[1],
        .pin3 = outputPins[2],
        .pin4 = outputPins[3],
    };
}
