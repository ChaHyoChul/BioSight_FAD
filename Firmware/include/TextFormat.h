#ifndef TEXTFORMAT_H_
#define TEXTFORMAT_H_

#include <stdint.h>

char *WriteText(char *out, const char *limit, const char *text);
char *WriteHex(char *out, uint16_t value, uint8_t digits);
char *WriteDecimal(char *out, int16_t value, uint8_t minimumDigits);
char *WriteHundredths(char *out, float value);

#endif
