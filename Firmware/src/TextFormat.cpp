#include "TextFormat.h"
#include <string.h>

#define FLOAT_SIGN_BIT (0x80000000UL)
#define FLOAT_INFINITY_BITS (0x7F800000UL)
#define FLOAT_MANTISSA_MASK (0x007FFFFFUL)
#define FLOAT_HIDDEN_BIT (0x00800000UL)
#define FLOAT_EXPONENT_BIAS (127)
#define FLOAT_MANTISSA_BITS (23)
#define MAX_HUNDREDTHS (999999UL)
#define DECIMAL_DIGIT_COUNT (5)
#define DIVIDE_BY_100_MULTIPLIER (5243UL)
#define DIVIDE_BY_100_SHIFT (19)
#define DIVIDE_BY_10_MULTIPLIER (205U)
#define DIVIDE_BY_10_SHIFT (11)

static_assert(sizeof(float) == sizeof(uint32_t), "Number formatting reads IEEE single precision bits");

static const char HEX_DIGITS[] = "0123456789ABCDEF";
static const uint16_t DECIMAL_POWERS[DECIMAL_DIGIT_COUNT] = { 10000U, 1000U, 100U, 10U, 1U };
static const uint32_t LARGE_DECIMAL_POWERS[] = { 100000UL, 10000UL };
static const uint16_t BITS[] = { 1U, 2U, 4U, 8U, 16U, 32U, 64U, 128U, 256U, 512U, 1024U, 2048U, 4096U, 8192U, 16384U, 32768U };

char *WriteText(char *out, const char *limit, const char *text)
{
    while(*text != 0 && out < limit)
    {
        *out++ = *text++;
    }
    return out;
}

char *WriteHex(char *out, uint16_t value, uint8_t digits)
{
    for(uint8_t i = digits; i > 0; i--)
    {
        out[i - 1] = HEX_DIGITS[value & 0x0FU];
        value = (uint16_t)(value >> 4);
    }
    return out + digits;
}

char *WriteDecimal(char *out, int16_t value, uint8_t minimumDigits)
{
    uint16_t magnitude = (uint16_t)value;
    if(value < 0)
    {
        *out++ = '-';
        magnitude = (uint16_t)(0U - magnitude);
    }

    bool started = false;
    for(uint8_t i = 0; i < DECIMAL_DIGIT_COUNT; i++)
    {
        uint16_t power = DECIMAL_POWERS[i];
        char digit = '0';
        while(magnitude >= power)
        {
            magnitude = (uint16_t)(magnitude - power);
            digit++;
        }

        started = started || digit != '0' || DECIMAL_DIGIT_COUNT - i <= minimumDigits;
        if(started)
        {
            *out++ = digit;
        }
    }

    return out;
}

static char *WriteTwoDigits(char *out, uint8_t value, bool leadingZero)
{
    uint8_t tens = (uint8_t)((value * DIVIDE_BY_10_MULTIPLIER) >> DIVIDE_BY_10_SHIFT);
    if(leadingZero || tens != 0)
    {
        *out++ = (char)('0' + tens);
    }
    *out++ = (char)('0' + value - tens * 10U);
    return out;
}

static uint32_t RoundToHundredths(uint32_t magnitude)
{
    uint8_t exponent = (uint8_t)(magnitude >> FLOAT_MANTISSA_BITS);
    if(exponent == 0)
    {
        return 0;
    }

    int16_t shift = (int16_t)(FLOAT_EXPONENT_BIAS + FLOAT_MANTISSA_BITS - exponent);
    if(shift > 31)
    {
        return 0;
    }

    if(shift <= 0)
    {
        return MAX_HUNDREDTHS;
    }

    uint32_t scaled = ((magnitude & FLOAT_MANTISSA_MASK) | FLOAT_HIDDEN_BIT) * 100UL;
    if(shift <= 16)
    {
        uint32_t quotient = scaled >> shift;
        uint32_t remainder = scaled & ((1UL << shift) - 1);
        uint32_t half = 1UL << (shift - 1);

        if(remainder > half || (remainder == half && (quotient & 1) != 0))
        {
            quotient++;
        }

        return quotient < MAX_HUNDREDTHS ? quotient : MAX_HUNDREDTHS;
    }

    uint8_t rest = (uint8_t)(shift - 16);
    uint16_t high = (uint16_t)(scaled >> 16);
    uint16_t low = (uint16_t)scaled;
    uint16_t quotient = (uint16_t)(((uint32_t)high * BITS[16 - rest]) >> 16);
    uint16_t remainder = (uint16_t)(high & (BITS[rest] - 1U));
    uint16_t half = BITS[rest - 1];

    if(remainder > half || (remainder == half && (low != 0 || (quotient & 1U) != 0)))
    {
        quotient++;
    }

    return quotient;
}

char *WriteHundredths(char *out, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));

    uint32_t magnitude = bits & ~FLOAT_SIGN_BIT;
    if((bits & FLOAT_SIGN_BIT) != 0 && magnitude - 1 < FLOAT_INFINITY_BITS)
    {
        *out++ = '-';
    }

    uint32_t hundredths = RoundToHundredths(magnitude);
    bool started = hundredths >= LARGE_DECIMAL_POWERS[1];

    if(started)
    {
        uint8_t high = 0;
        for(uint8_t i = 0; i < sizeof(LARGE_DECIMAL_POWERS) / sizeof(LARGE_DECIMAL_POWERS[0]); i++)
        {
            high = (uint8_t)(high * 10U);
            while(hundredths >= LARGE_DECIMAL_POWERS[i])
            {
                hundredths -= LARGE_DECIMAL_POWERS[i];
                high++;
            }
        }

        out = WriteTwoDigits(out, high, false);
    }

    uint16_t rest = (uint16_t)hundredths;
    uint8_t whole = (uint8_t)(((uint32_t)rest * DIVIDE_BY_100_MULTIPLIER) >> DIVIDE_BY_100_SHIFT);
    out = WriteTwoDigits(out, whole, started);
    *out++ = '.';
    return WriteTwoDigits(out, (uint8_t)(rest - whole * 100U), true);
}
