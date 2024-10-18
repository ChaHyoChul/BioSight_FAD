#include "ChecksumCalculator.h"

ChecksumCalculator::ChecksumCalculator()
{
}

ChecksumCalculator::~ChecksumCalculator()
{
}

byte ChecksumCalculator::CalculateChecksum(char* buffer, int length)
{
    int checksum = 0;

    for(int i = 0; i < length; i++)
    {
        checksum += buffer[i];
    }

    return (byte)(checksum & 0xFF);
}
