#ifndef CHECKSUMCALCULATOR_H_
#define CHECKSUMCALCULATOR_H_

#include <stdint.h>

uint8_t CalculateChecksum(const char *buffer, int length);

#endif
