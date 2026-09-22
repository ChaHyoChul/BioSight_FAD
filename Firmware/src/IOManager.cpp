#include "IOManager.h"
#include "Board.h"
#include "Global.h"

#define CLICK_STATE_CHANGE_COUNT (2)

void IOManager::Process()
{
    ProcessDigitalInputs(Board::ReadDigitalInputs());
    ProcessDigitalOutputs(Board::ReadDigitalOutputs());
    ProcessAnalogInputs();
}

void IOManager::ProcessDigitalInputs(uint16_t states)
{
    if(states == _committedInputs && _activeTimerCount == 0)
    {
        return;
    }

    for(uint8_t index = 0; index < DIGITAL_INPUT_COUNT; index++, states >>= 1)
    {
        bool newState = (states & 0x01) != 0;
        Timer &timer = _inputChatteringTimer[index];

        if(!timer.IsStarted())
        {
            if(global._digitalInputs[index] != newState)
            {
                timer.Start(INPUT_CHATTERING_TIMEOUT_MILLISECONDS);
                _activeTimerCount++;
            }
            continue;
        }

        if(!timer.IsTimeout())
        {
            continue;
        }

        timer.Reset();
        _activeTimerCount--;

        if(global._digitalInputs[index] == newState)
        {
            _changedStateCountInputs[index] = 0;
            continue;
        }

        global._digitalInputs[index] = newState;
        _committedInputs = (uint16_t)(_committedInputs ^ (1u << index));

        if(global._controlMode != ControlMode::CM_Manual)
        {
            continue;
        }

        _changedStateCountInputs[index]++;

        if(_changedStateCountInputs[index] >= CLICK_STATE_CHANGE_COUNT)
        {
            global._digitalInputsClicked[index] = !global._digitalInputsClicked[index];
            _changedStateCountInputs[index] = 0;
        }
    }
}

void IOManager::ProcessDigitalOutputs(uint8_t states)
{
    if(_hasOutputStates && states == _outputStates)
    {
        return;
    }

    _outputStates = states;
    _hasOutputStates = true;

    for(uint8_t index = 0; index < DIGITAL_OUTPUT_COUNT; index++, states >>= 1)
    {
        global._digitalOutputs[index] = (states & 0x01) != 0;
    }
}

void IOManager::ProcessAnalogInputs()
{
    uint8_t sampleCount = Board::AnalogSampleCount();
    if(_hasAnalogValues && sampleCount == _analogSampleCount)
    {
        return;
    }

    _analogSampleCount = sampleCount;

    for(uint8_t index = 0; index < ANALOG_INPUT_COUNT; index++)
    {
        uint16_t raw = Board::ReadAnalogInput(index);
        if(_hasAnalogValues && raw == _analogRawValues[index])
        {
            continue;
        }

        _analogRawValues[index] = raw;
        global._analogInputs[index] = raw * ANALOG_INPUT_FULL_SCALE / ANALOG_INPUT_RESOLUTION;
    }

    _hasAnalogValues = true;
}
