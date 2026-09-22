#include "KiSAN_KM6023.h"
#include "Global.h"
#include "TextFormat.h"
#include <stdlib.h>
#include <string.h>

#define KM6023_RAW_SCALE (1000.0f)
#define KM6023_WRITE_PAYLOAD ("S0204")
#define KM6023_FIELD_DIGITS (4)

static_assert(sizeof(KM6023_WRITE_PAYLOAD) - 1 + ANALOG_INPUT_COUNT * (KM6023_FIELD_DIGITS + 1) < KISAN_PAYLOAD_BUFFER_SIZE, "KM6023 payload must fit");

KiSAN_KM6023::KiSAN_KM6023(SerialPort &serial)
: KiSANModule(serial, KISAN_ADDRESS_KM6023)
{
}

void KiSAN_KM6023::Send()
{
    char payload[KISAN_PAYLOAD_BUFFER_SIZE] = {};
    char *out = WriteText(payload, payload + sizeof(payload) - 1, KM6023_WRITE_PAYLOAD);

    for(uint8_t i = 0; i < ANALOG_INPUT_COUNT; i++)
    {
        *out++ = ',';
        out = WriteHex(out, (uint16_t)(global._analogInputs[i] * KM6023_RAW_SCALE), KM6023_FIELD_DIGITS);
    }

    *out = 0;
    SendFrame(payload);
}

void KiSAN_KM6023::ProcessReceive()
{
    char *token = strtok(_receiveBuffer, ",");

    int index = 0;
    while(token != NULL && index < KISAN_ANALOG_OUTPUT_COUNT)
    {
        token = strtok(NULL, ",");
        if(token == NULL || token[0] == 0)
        {
            break;
        }

        unsigned long data = strtoul(token, NULL, 16);
        global._analogOutputsKisan[index] = data / KM6023_RAW_SCALE;
        index++;
    }
}
