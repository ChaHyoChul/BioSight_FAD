#ifndef BOARD_H_
#define BOARD_H_

#include <stdint.h>

namespace Board
{
    void Initialize();
    uint32_t Millis();

    uint16_t ReadDigitalInputs();
    uint8_t ReadDigitalOutputs();
    void WriteDigitalOutput(uint8_t index, bool state);
    void WriteDigitalOutputs(uint8_t states);

    uint16_t ReadAnalogInput(uint8_t channel);
    uint8_t AnalogSampleCount();

    void ToggleStatusLed();
}

#endif
