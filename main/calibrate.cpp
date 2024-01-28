#include "calibrate.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "letters.hpp"

// Get input from serial, save to buf up to max len
static void getLineInput(char buf[], size_t len, unsigned long timeoutMillis = 86400000);

// Get numeric input from the user, using buf for the raw user input, up to len
static int getNumberInput(char buf[], size_t len);

// Get a yes/no answer from the user
static bool getYesNoInput(char buf[], size_t len, bool defaultValue);

// Get a yes/no answer from the user, but only give user a limited time to respond
static bool getYesNoInputTimed(char buf[], size_t len, bool defaultValue, uint32_t waitMillis);

// Get the number of milliseconds since boot
static unsigned long getMillis();

static const char *TAG = "CALIBRATE";

int UnitCalibration::getPositionForCharacter(char letter) {
    // A-Z
    if (letter >= 65 && letter <= 90) {
        return getPosition(letter - 65 + unitLettersLettersStart);
    }

    // a-z
    if (letter >= 97 && letter <= 122) {
        return getPosition(letter - 97 + unitLettersLettersStart);
    }

    // 0-9
    if (letter >= 48 && letter <= 57) {
        return getPosition(letter - 48 + unitLettersNumbersStart);
    }

    // Naive search
    for (uint8_t i = 0; i < unitLettersCount; i++) {
        if (letter == unitLetters[i])
            return getPosition(i);
    }

    // Failsafe
    ESP_LOGW(TAG, "Unsupported letter %d", letter);
    return getPosition(0);
}

Calibrate::Calibrate(MultiStepper *units)
: _units(units) {
    ESP_ERROR_CHECK(nvs_open("calibration", NVS_READWRITE, &_nvs));
}

void Calibrate::populateCalibrations(UnitCalibration *unitCalibrations, int numUnits) {
    for (int i = 0; i < numUnits; i++)
        unitCalibrations[i] = readFromNvs(i);
}

void Calibrate::calibrate() {
    bool setupComplete = getNvsBool("complete", false);
    if (setupComplete) {
        std::cout << "Setup has already been completed. Would you like to rerun setup? [y/N]:\n";
        std::cout << "If no response is provided in 5 seconds, the existing calibration will be used\n";
        bool redoSetup = getYesNoInputTimed(_lineBuf, _lineBufLen, false, 5000);
        if (!redoSetup) {
            std::cout << "Continuing standard boot sequence";
            return;
        }
    }

    std::cout << "We're now going to calibrate each letter\n";

    for (uint8_t i = 0; i < _units->getNumUnits(); i++)
        calibrateUnit(i);

    setNvsBool("complete", true);
    std::cout << "Calibration of all units complete!";
}

UnitCalibration Calibrate::calibrateUnit(uint8_t unitNum) {
    std::cout << "We're calibrating unit: " << unitNum + 1 << '\n';

    // Check if we've already got calibration data for this unit
    UnitCalibration calibrationData = readFromNvs(unitNum);
    if (calibrationData.getFirstLetterPosition() > 0) {
        std::cout << "Existing calibration data found:\n";
        std::cout << "First letter position: " << calibrationData.getFirstLetterPosition() << '\n';
        std::cout << "Steps between flaps: " << calibrationData.getStepsBetweenFlaps() << '\n';

        std::cout << "Would you like to use the existing calibration data? [Y/n]:\n";
        bool useExisting = getYesNoInput(_lineBuf, _lineBufLen, true);
        if (useExisting) {
            std::cout << "Using existing calibration data for unit " << unitNum << "\n";
            return calibrationData;
        }
    }

    // Get initial ballpark values
    calibrationData = initialHomeCalibration(unitNum);

    // Move 50% into the flap 'area' so that we have tolerance
    calibrationData = UnitCalibration(
        calibrationData.getFirstLetterPosition() + (calibrationData.getStepsBetweenFlaps() / 2), 
        calibrationData.getStepsBetweenFlaps()
    );

    // Perform final calibration data
    calibrationData = refineCalibration(unitNum, calibrationData);

    // Let user know values, and save to long-term storage
    std::cout << "First letter position: " << calibrationData.getFirstLetterPosition() << '\n';
    std::cout << "Steps between flaps: " << calibrationData.getStepsBetweenFlaps() << '\n';
    saveToNvs(unitNum, calibrationData);    
    
    return calibrationData;
}

UnitCalibration Calibrate::initialHomeCalibration(uint8_t unitNum) {
    int targetPosition = 0;
    std::cout << "You will be asked to answer yes or no whether a flap is visible.\n";
    std::cout << "The default is NO if you press enter. Type in y to answer yes to any question.\n";
    std::cout << "Don't worry if you overrun by 1 step, this won't impact calibration.\n";
    std::cout << "There will also be coloured log output, you can ignore this except for debugging.\n";

    // Locate flap directly prior
    while (true) {
        std::cout << "Is '!' showing? [y/N]\n";
        bool response = getYesNoInput(_lineBuf, _lineBufLen, false);
        if (response) {
            break;
        }

        std::cout << "Moving 5 steps\n";
        targetPosition += 5;
        _units->setTargetPosition(unitNum, targetPosition);
        _units->moveAllUnits();
    }

    std::cout << "Excellent! We now need to move to the very start of the next flap";
    ++targetPosition;

    while (true) {
        _units->setTargetPosition(unitNum, targetPosition);
        _units->moveAllUnits();

        std::cout << "Is a blank flap showing? [y/N]\n";
        bool response = getYesNoInput(_lineBuf, _lineBufLen, false);
        if (response) {
            break;
        }

        std::cout << "Moving 1 step\n";
        ++targetPosition;
    }

    int spacePosition = targetPosition;
    std::cout << "Excellent! We need to repeat the process one more time";
    ++targetPosition;

    while (true) {
        _units->setTargetPosition(unitNum, targetPosition);
        _units->moveAllUnits();

        std::cout << "Is 'A' showing? [y/N]\n";
        bool response = getYesNoInput(_lineBuf, _lineBufLen, false);
        if (response) {
            break;
        }

        std::cout << "Moving 1 step\n";
        ++targetPosition;
    }

    int nextFlapPosition = targetPosition;
    int flapsDelta = nextFlapPosition - spacePosition;

    return UnitCalibration(spacePosition, flapsDelta);
}

UnitCalibration Calibrate::refineCalibration(uint8_t unitNum, UnitCalibration calibrationData) {
    for (int i = 0; i < calLettersCount; i++) {
        // Move to letter
        int targetNum = calLetters[i];
        int position = calibrationData.getPosition(targetNum);
        _units->setTargetPosition(unitNum, position);
        _units->moveAllUnits();

        // Confirm correct letter showing
        std::cout << "\nIs '" << unitLetters[calLetters[i]] << "' showing? [y/N]\n";
        bool letterShowing = getYesNoInput(_lineBuf, _lineBufLen, false);
        if (letterShowing)
            continue;

        // Get new values from user
        std::cout << "\nFor the first unit, favour changing the steps between flaps\n";
        std::cout << "For subsequent units, try the same steps as the first unit and adjust the offset\n";
        std::cout << "The numbers in brackets are the *maximum* suggested change per try, lower changes often make sense.\n";
        std::cout << "Current offset is: " << calibrationData.getFirstLetterPosition() << "\n";
        std::cout << "Current steps between flaps is: " << calibrationData.getStepsBetweenFlaps() << "\n";
        std::cout << "Enter a new value for offset (+- 5):\n";
        int newOffset = getNumberInput(_lineBuf, _lineBufLen);
        std::cout << "\nEnter a new value for the steps (+- 3):\n";
        int newSteps = getNumberInput(_lineBuf, _lineBufLen);
        calibrationData = UnitCalibration(newOffset, newSteps);
        if (i > 0)
            i -= 2;
        else
            --i;
    }

    std::cout << "\nCalibration sequence complete!\n";
    return calibrationData;
}

void Calibrate::saveToNvs(uint8_t unitNum, UnitCalibration calibration) {
    char keyBuffer[NVS_KEY_NAME_MAX_SIZE];
    sprintf(keyBuffer, "unit:%d:fl", unitNum);
    setNvsInt(keyBuffer, calibration.getFirstLetterPosition());
    sprintf(keyBuffer, "unit:%d:stp", unitNum);
    setNvsInt(keyBuffer, calibration.getStepsBetweenFlaps());
}

UnitCalibration Calibrate::readFromNvs(uint8_t unitNum) {
    char keyBuffer[NVS_KEY_NAME_MAX_SIZE];
    sprintf(keyBuffer, "unit:%d:fl", unitNum);
    int firstLetterPosition = getNvsInt(keyBuffer, 0);
    sprintf(keyBuffer, "unit:%d:stp", unitNum);
    int stepsBetweenFlaps = getNvsInt(keyBuffer, 0);

    return UnitCalibration(firstLetterPosition, stepsBetweenFlaps);
}

bool Calibrate::getNvsBool(const char *key, bool defaultValue) {
    uint8_t rawValue = 0;
    esp_err_t err = nvs_get_u8(_nvs, key, &rawValue);

    switch (err) {
        case ESP_OK:
            return rawValue > 0;
        case ESP_ERR_NVS_NOT_FOUND:
            return defaultValue;
        default:
            // Should throw error
            ESP_ERROR_CHECK(err);
            return defaultValue;
    }
}

void Calibrate::setNvsBool(const char *key, bool value) {
    uint8_t rawValue = value ? 1 : 0;
    ESP_ERROR_CHECK(nvs_set_u8(_nvs, key, rawValue));
}

int32_t Calibrate::getNvsInt(const char *key, int32_t defaultValue) {
    int32_t rawValue = 0;
    esp_err_t err = nvs_get_i32(_nvs, key, &rawValue);

    switch (err) {
        case ESP_OK:
            return rawValue;
        case ESP_ERR_NVS_NOT_FOUND:
            return defaultValue;
        default:
            // Should throw error
            ESP_ERROR_CHECK(err);
            return defaultValue;
    }
}

void Calibrate::setNvsInt(const char *key, int32_t value) {
    ESP_ERROR_CHECK(nvs_set_i32(_nvs, key, value));
}

static bool getYesNoInput(char buf[], size_t len, bool defaultValue) {
    while (true) {
        getLineInput(buf, len);
        if (buf[0] == 'Y' || buf[0] == 'y')
            return true;
        if (buf[0] == 'N' || buf[0] == 'n')
            return false;
        if (buf[0] == 0 || buf[0] == '\n')
            return defaultValue;

        std::cout << "Invalid input, enter 'y' or 'n' as a single character:\n";
    }
}

static bool getYesNoInputTimed(char buf[], size_t len, bool defaultValue, uint32_t waitMillis) {
    while (true) {
        getLineInput(buf, len, waitMillis);
        if (buf[0] == 'Y' || buf[0] == 'y')
            return true;
        if (buf[0] == 'N' || buf[0] == 'n')
            return false;
        if (buf[0] == 0 || buf[0] == '\n')
            return defaultValue;

        std::cout << "Invalid input, enter 'y' or 'n' as a single character:\n";
    }
}

static int getNumberInput(char buf[], size_t len) {
    getLineInput(buf, len);
    return atoi(buf);
}

// https://stackoverflow.com/a/74227146
static void getLineInput(char buf[], size_t len, unsigned long timeoutMillis)
{
    unsigned long startTime = getMillis();

    memset(buf, 0, len);
    fpurge(stdin); //clears any junk in stdin
    char *bufp;
    bufp = buf;
    while(true) {
        vTaskDelay(20/portTICK_PERIOD_MS);
        *bufp = getchar();
        if(*bufp != '\0' && *bufp != 0xFF && *bufp != '\r') //ignores null input, 0xFF, CR in CRLF
        {
            //'enter' (EOL) handler 
            if (*bufp == '\n'){
                *bufp = '\0';
                std::cout << *bufp;
                break;
            } //backspace handler
            else if (*bufp == '\b') {
                if(bufp-buf >= 1)
                    bufp--;
                std::cout << '\n' << bufp;
            }
            else {
                std::cout << *bufp;
                //pointer to next character
                bufp++;
            }
        }
        
        //only accept len-1 characters, (len) character being null terminator.
        if (bufp-buf > (len)-2) {
            bufp = buf + (len -1);
            *bufp = '\0';
            break;
        }

        // Check if timeout hit
        unsigned long now = getMillis();
        if (now - startTime > timeoutMillis) {
            buf[0] = '\n';
            buf[1] = 0;
            return;
        }
    }
}

static unsigned long getMillis() {
    return (unsigned long) (esp_timer_get_time() / 1000ULL);
}
