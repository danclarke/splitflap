#pragma once

#include "multistepper.hpp"
#include "nvs_flash.h"
#include "nvs.h"

class UnitCalibration {
    public:
        UnitCalibration() {}
        UnitCalibration(int firstLetterPosition, int stepsBetweenFlaps)
        : _firstLetterPosition(firstLetterPosition), _stepsBetweenFlaps(stepsBetweenFlaps) {}

        int getFirstLetterPosition() { return _firstLetterPosition; };
        int getStepsBetweenFlaps() { return _stepsBetweenFlaps; };
        int getPosition(int letterNum) { return _firstLetterPosition + (_stepsBetweenFlaps * letterNum); }
        int getPositionForCharacter(char letter);

    private:
        int _firstLetterPosition = 0;
        int _stepsBetweenFlaps = 0;
};

class Calibrate {
    public:
        Calibrate(MultiStepper *units);

        // Run the user through the calibration process
        // Will auto-load where possible and prompt user with option to rerun
        void calibrate();

        void populateCalibrations(UnitCalibration *unitCalibrations, int numUnits);
    
    private:
        UnitCalibration calibrateUnit(uint8_t unitNum);
        UnitCalibration initialHomeCalibration(uint8_t unitNum);
        UnitCalibration refineCalibration(uint8_t unitNum, UnitCalibration calibrationData);

        void saveToNvs(uint8_t unitNum, UnitCalibration calibration);
        UnitCalibration readFromNvs(uint8_t unitNum);
        bool getNvsBool(const char *key, bool defaultValue);
        void setNvsBool(const char *key, bool value);
        int32_t getNvsInt(const char *key, int32_t defaultValue);
        void setNvsInt(const char *key, int32_t value);

        MultiStepper *_units;
        char _lineBuf[1024];
        size_t _lineBufLen = 1024;
        nvs_handle_t _nvs;
};