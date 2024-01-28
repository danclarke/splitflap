#include "multistepper.hpp"
#include "esp_log.h"
#include "esp_check.h"
#include "rom/ets_sys.h"
#include <memory>

static const char* TAG = "MULTISTEPPER";

// Handle timer callback for stepper motor delay
// Each callback should be a step, if a step is required
static bool IRAM_ATTR timerHandler(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
    stepperTimerData_t *timerData = (stepperTimerData_t*)user_ctx;
    BaseType_t higherTaskAwoken = pdFALSE;
    
    xSemaphoreGiveFromISR(timerData->semaphore, &higherTaskAwoken);
    return higherTaskAwoken == pdTRUE;
}

MultiStepper::MultiStepper(Stepper *steppers, uint8_t numSteppers, gpio_num_t pinEn, gpio_num_t pinLatch, gpio_num_t pinData, gpio_num_t pinClk, uint64_t stepDelayUs) 
: _steppers(steppers), _numSteppers(numSteppers), _pinEn(pinEn), _pinLatch(pinLatch), _pinData(pinData), _pinClk(pinClk), _stepDelay(stepDelayUs) {
    // Set up the pins for the 74HC595
    gpio_reset_pin(pinEn);
    ESP_ERROR_CHECK(gpio_set_direction(pinEn, GPIO_MODE_OUTPUT_OD));
    gpio_reset_pin(pinLatch);
    ESP_ERROR_CHECK(gpio_set_direction(pinLatch, GPIO_MODE_OUTPUT));
    gpio_reset_pin(pinData);
    ESP_ERROR_CHECK(gpio_set_direction(pinData, GPIO_MODE_OUTPUT));
    gpio_reset_pin(pinClk);
    ESP_ERROR_CHECK(gpio_set_direction(pinClk, GPIO_MODE_OUTPUT));

    // Zero out everything
    zeroMotors();

    // Enable the shift registers, since they're all now 0
    ESP_ERROR_CHECK(gpio_set_level(_pinEn, 0));
    ESP_LOGI(TAG, "%d stepper motors initialised", numSteppers);

    // Get ready for step delays
    setupTimer();
}

MultiStepper::~MultiStepper() {
    // We don't want to leave orphan timers running
    ESP_ERROR_CHECK(gptimer_stop(_timer));
    ESP_ERROR_CHECK(gptimer_disable(_timer));
    ESP_ERROR_CHECK(gptimer_del_timer(_timer));
}

int MultiStepper::getNumUnits() {
    return _numSteppers;
}

void MultiStepper::setTargetPosition(uint8_t unitNumber, int position) {
    _steppers[unitNumber].setTarget(position);
}

int MultiStepper::getUnitPosition(uint8_t unitNumber) {
    return _steppers[unitNumber].getPosition();
}

void MultiStepper::moveAllUnits() {
    if (!_homed)
        home();

    moveToTarget();
}

void MultiStepper::home() {
    ESP_LOGI(TAG, "Performing initial move to magnet");
    moveToMagnet();

    ESP_LOGI(TAG, "Moving away from magnet");
    int targetPosition = 200;
    for (uint8_t i = 0; i < _numSteppers; i++)
        _steppers[i].setTarget(targetPosition);
    moveToTarget();

    ESP_LOGI(TAG, "Moving back to magnet");
    moveToMagnet();

    _homed = true;
    ESP_LOGI(TAG, "All motors homed");
}

void MultiStepper::moveToMagnet() {
    ESP_ERROR_CHECK(gptimer_start(_timer));

    uint8_t numMotorsHomed = 0;
    std::unique_ptr<bool[]> motorsHomed(new bool[_numSteppers]);

    // Set a crazy high initial step value so we'll definitely hit home
    for (uint8_t i = 0; i < _numSteppers; i++) {
        _steppers[i].setTarget(10000);
        motorsHomed[i] = false;
    }

    // Step each motor until we hit home
    while (numMotorsHomed < _numSteppers) {
        rolloutPins();

        // Check each motor, once homed stop the movement and register success
        for (uint8_t i = 0; i < _numSteppers; i++) {
            // If already homed, ignore this motor
            if (motorsHomed[i])
                continue;

            // Motor is now homed, add it to the list
            if (_steppers[i].hallActive()) {
                ESP_LOGI(TAG, "Motor %d is homed", i + 1);
                ++numMotorsHomed;
                motorsHomed[i] = true;
                _steppers[i].setTarget(0);
            }
        }

        xSemaphoreTake(_timerData.semaphore, portMAX_DELAY);
    }

    ESP_ERROR_CHECK(gptimer_stop(_timer));
}

void MultiStepper::moveToTarget() {
    ESP_ERROR_CHECK(gptimer_start(_timer));

    uint8_t numMotorsAtTarget = 0;
    std::unique_ptr<bool[]> motorAtTarget(new bool[_numSteppers]);

    for (uint8_t i = 0; i < _numSteppers; i++)
        motorAtTarget[i] = false;

    // Step each motor until we hit home
    while (numMotorsAtTarget < _numSteppers) {
        rolloutPins();

        // Check each motor
        for (uint8_t i = 0; i < _numSteppers; i++) {
            // If already at position, ignore this motor
            if (motorAtTarget[i])
                continue;

            // Motor is now at position, add it to the list
            if (_steppers[i].isAtTarget()) {
                ESP_LOGI(TAG, "Motor %d is at target position", i + 1);
                ++numMotorsAtTarget;
                motorAtTarget[i] = true;
            }
        }

        xSemaphoreTake(_timerData.semaphore, portMAX_DELAY);
    }

    // We're in position, so we can turn off all the motors
    zeroMotors();
    ESP_ERROR_CHECK(gptimer_stop(_timer));
}

void MultiStepper::rolloutPins() {
    startShift();

    // Motors are daisy chained with shift registers, so when we push a value on it'll shift to the end
    // Therefore we start from the last pin of the last motor, and work backwards
    for (int i = _numSteppers - 1; i >= 0; i--) {
        StepperPins_t pinStates = _steppers[i].step();
        shiftOut(pinStates.pin4);
        shiftOut(pinStates.pin3);
        shiftOut(pinStates.pin2);
        shiftOut(pinStates.pin1);
    }

    endShift();
}

void MultiStepper::startShift() {
    ESP_ERROR_CHECK(gpio_set_level(_pinLatch, 0));
}

void MultiStepper::endShift() {
    ESP_ERROR_CHECK(gpio_set_level(_pinLatch, 1));
}

void MultiStepper::shiftOut(bool value) {
    ESP_ERROR_CHECK(gpio_set_level(_pinData, value ? 1 : 0));
    ESP_ERROR_CHECK(gpio_set_level(_pinClk, 1));
    ESP_ERROR_CHECK(gpio_set_level(_pinClk, 0));
}

void MultiStepper::zeroMotors() {
    int totalPins = (int)_numSteppers * 4;
    startShift();
    for (int i = 0; i < totalPins; i++)
        shiftOut(0);
    endShift();
}

void MultiStepper::setupTimer() {
    // Setup timer so we have fine-grained delay times without cannibalising the CPU with spinlocks
    // This code is a bit dirty / hacky
    _timerData.semaphore = xSemaphoreCreateBinary();

    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1 * 1000 * 1000, /* 1MHz, 1 tick = 1us */
        .intr_priority = 0,
        .flags = {
            .intr_shared = 1,
        },
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &_timer));

    gptimer_alarm_config_t alarmConfig;
    alarmConfig.reload_count = 0;
    alarmConfig.alarm_count = _stepDelay;
    alarmConfig.flags.auto_reload_on_alarm = true;
    ESP_ERROR_CHECK(gptimer_set_alarm_action(_timer, &alarmConfig));

    gptimer_event_callbacks_t cbs = {
        .on_alarm = timerHandler, // register user callback
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(_timer, &cbs, &_timerData));
    ESP_ERROR_CHECK(gptimer_enable(_timer));
}