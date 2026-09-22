#ifndef IOMANAGER_H_
#define IOMANAGER_H_

#include <stdint.h>
#include "GlobalDefinition.h"
#include "Timer.h"

class IOManager
{
private:
    uint8_t _changedStateCountInputs[DIGITAL_INPUT_COUNT] = {};
    Timer _inputChatteringTimer[DIGITAL_INPUT_COUNT];
    uint16_t _committedInputs = 0;
    uint8_t _activeTimerCount = 0;

    uint8_t _outputStates = 0;
    bool _hasOutputStates = false;

    uint16_t _analogRawValues[ANALOG_INPUT_COUNT] = {};
    uint8_t _analogSampleCount = 0;
    bool _hasAnalogValues = false;

public:
    void Process();

private:
    void ProcessDigitalInputs(uint16_t states);
    void ProcessDigitalOutputs(uint8_t states);
    void ProcessAnalogInputs();
};

#endif
