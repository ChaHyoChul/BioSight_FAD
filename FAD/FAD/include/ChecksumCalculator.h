#ifndef CHECKSUMCALCULATOR_H_
#define CHECKSUMCALCULATOR_H_

#include <Arduino.h>

class ChecksumCalculator
{
public:
    ChecksumCalculator();
    ~ChecksumCalculator();

public:
    byte CalculateChecksum(char* buffer, int length);
};

#endif
