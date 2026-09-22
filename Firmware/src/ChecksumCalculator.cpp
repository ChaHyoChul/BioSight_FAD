#include "ChecksumCalculator.h"

uint8_t CalculateChecksum(const char *buffer, int length)
{
    uint8_t checksum = 0;

    for(int i = 0; i < length; i++)
    {
        checksum = (uint8_t)(checksum + (uint8_t)buffer[i]);
    }

    return checksum;
}
