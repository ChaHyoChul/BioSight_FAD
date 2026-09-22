#ifndef HOSTBOARD_H_
#define HOSTBOARD_H_

#include <stdint.h>
#include <string>
#include "GlobalDefinition.h"
#include "SerialPort.h"

namespace HostBoard
{
    extern uint32_t Millis;
    extern uint16_t Inputs;
    extern uint8_t Outputs;
    extern uint16_t Analog[ANALOG_INPUT_COUNT];
    extern uint32_t LedToggles;

    void Reset();
    void Feed(SerialPort &port, const std::string &data);
    std::string Drain(SerialPort &port);
}

#endif
