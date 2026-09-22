#ifndef GLOBAL_H_
#define GLOBAL_H_

#include <stdint.h>
#include "GlobalDefinition.h"
#include "SystemMode.h"
#include "ControlMode.h"

class Global
{
public:
    SystemMode _systemMode = SystemMode::SM_Release;
    ControlMode _controlMode = ControlMode::CM_Undefined;

    bool _digitalInputs[DIGITAL_INPUT_COUNT] = {};
    bool _digitalInputsClicked[DIGITAL_INPUT_COUNT] = {};
    bool _digitalOutputs[DIGITAL_OUTPUT_COUNT] = {};
    float _analogInputs[ANALOG_INPUT_COUNT] = {};

    float _analogInputsKisan[KISAN_ANALOG_INPUT_COUNT] = {};
    bool _kisanOutputs[KISAN_DIGITAL_OUTPUT_COUNT] = {};
    float _analogOutputsKisan[KISAN_ANALOG_OUTPUT_COUNT] = {};
    float _analogOutputsKisanTargets[KISAN_ANALOG_OUTPUT_COUNT] = {};

    float _oxygenConcentration = 0.0f;
};

extern Global global;

uint16_t PackFlags(const bool *flags, uint8_t count);

#endif
