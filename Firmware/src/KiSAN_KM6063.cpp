#include "KiSAN_KM6063.h"
#include "Global.h"
#include "TextFormat.h"

#define KM6063_WRITE_PAYLOAD ("S0001")
#define KM6063_FIELD_DIGITS (4)

static_assert(sizeof(KM6063_WRITE_PAYLOAD) - 1 + 1 + KM6063_FIELD_DIGITS < KISAN_PAYLOAD_BUFFER_SIZE, "KM6063 payload must fit");

KiSAN_KM6063::KiSAN_KM6063(SerialPort &serial)
: KiSANModule(serial, KISAN_ADDRESS_KM6063)
{
}

void KiSAN_KM6063::Send()
{
    char payload[KISAN_PAYLOAD_BUFFER_SIZE] = {};
    char *out = WriteText(payload, payload + sizeof(payload) - 1, KM6063_WRITE_PAYLOAD);
    *out++ = ',';
    out = WriteHex(out, PackFlags(global._kisanOutputs, KISAN_DIGITAL_OUTPUT_COUNT), KM6063_FIELD_DIGITS);
    *out = 0;

    SendFrame(payload);
}

void KiSAN_KM6063::ProcessReceive()
{
}
