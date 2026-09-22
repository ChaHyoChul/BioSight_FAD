#include "KiSAN_KM6015.h"
#include "Global.h"
#include <stdlib.h>
#include <string.h>

#define KM6015_READ_PAYLOAD ("G8208")
#define KM6015_RAW_FULL_SCALE (65535.0f)

KiSAN_KM6015::KiSAN_KM6015(SerialPort &serial)
: KiSANModule(serial, KISAN_ADDRESS_KM6015)
{
}

void KiSAN_KM6015::Send()
{
    SendFrame(KM6015_READ_PAYLOAD);
}

void KiSAN_KM6015::ProcessReceive()
{
    char *token = strtok(_receiveBuffer, ",");

    int index = 0;
    while(token != NULL && index < KISAN_ANALOG_INPUT_COUNT)
    {
        token = strtok(NULL, ",");
        if(token == NULL || token[0] == 0)
        {
            break;
        }

        unsigned long data = strtoul(token, NULL, 16);
        global._analogInputsKisan[index] = (data * ANALOG_INPUT_FULL_SCALE) / KM6015_RAW_FULL_SCALE;
        index++;
    }
}
