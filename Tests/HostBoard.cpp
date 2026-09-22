#include "HostBoard.h"
#include "Board.h"
#include "SerialHardware.h"

namespace HostBoard
{
    uint32_t Millis = 0;
    uint16_t Inputs = 0;
    uint8_t Outputs = 0;
    uint16_t Analog[ANALOG_INPUT_COUNT] = {};
    uint32_t LedToggles = 0;

    static void Empty(SerialPort &port)
    {
        while(port.Read() >= 0)
        {
        }

        Drain(port);
    }

    void Reset()
    {
        Millis = 0;
        Inputs = 0;
        Outputs = 0;
        LedToggles = 0;

        for(uint16_t &value : Analog)
        {
            value = 0;
        }

        Empty(DebugSerial);
        Empty(HostSerial);
        Empty(FieldbusSerial);
    }

    void Feed(SerialPort &port, const std::string &data)
    {
        for(char ch : data)
        {
            port.OnReceive((uint8_t)ch);
        }
    }

    std::string Drain(SerialPort &port)
    {
        std::string data;
        uint8_t value;

        while(port.TakeTransmit(&value))
        {
            data.push_back((char)value);
        }

        return data;
    }
}

void Board::Initialize()
{
}

uint32_t Board::Millis()
{
    return HostBoard::Millis;
}

uint16_t Board::ReadDigitalInputs()
{
    return HostBoard::Inputs;
}

uint8_t Board::ReadDigitalOutputs()
{
    return HostBoard::Outputs;
}

void Board::WriteDigitalOutput(uint8_t index, bool state)
{
    uint8_t mask = (uint8_t)(1 << index);
    HostBoard::Outputs = state ? (uint8_t)(HostBoard::Outputs | mask) : (uint8_t)(HostBoard::Outputs & ~mask);
}

void Board::WriteDigitalOutputs(uint8_t states)
{
    HostBoard::Outputs = states;
}

uint16_t Board::ReadAnalogInput(uint8_t channel)
{
    return HostBoard::Analog[channel & (ANALOG_INPUT_COUNT - 1)];
}

uint8_t Board::AnalogSampleCount()
{
    static uint8_t samples = 0;
    samples = (uint8_t)(samples + 1);
    return samples;
}

void Board::ToggleStatusLed()
{
    HostBoard::LedToggles++;
}

void SerialHardwareBegin(uint8_t, uint32_t)
{
}

void SerialHardwareStartTransmit(uint8_t)
{
}
