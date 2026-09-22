#ifndef SERIALHARDWARE_H_
#define SERIALHARDWARE_H_

#include <stdint.h>

void SerialHardwareBegin(uint8_t index, uint32_t baudRate);
void SerialHardwareStartTransmit(uint8_t index);

#endif
