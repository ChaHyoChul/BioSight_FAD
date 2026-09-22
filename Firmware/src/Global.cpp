#include "Global.h"

Global global;

uint16_t PackFlags(const bool *flags, uint8_t count)
{
    uint16_t packed = 0;
    uint16_t bit = 1;
    for(uint8_t i = 0; i < count; i++, bit = (uint16_t)(bit << 1))
    {
        if(flags[i])
        {
            packed |= bit;
        }
    }
    return packed;
}
