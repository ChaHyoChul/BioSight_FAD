#include "SerialPort.h"
#include "SerialHardware.h"
#include <string.h>

#define RX_MASK (SERIAL_RX_BUFFER_SIZE - 1)
#define TX_MASK (SERIAL_TX_BUFFER_SIZE - 1)

static_assert((SERIAL_RX_BUFFER_SIZE & RX_MASK) == 0 && SERIAL_RX_BUFFER_SIZE <= 256, "RX buffer size must be a power of two up to 256");
static_assert((SERIAL_TX_BUFFER_SIZE & TX_MASK) == 0 && SERIAL_TX_BUFFER_SIZE <= 256, "TX buffer size must be a power of two up to 256");

SerialPort DebugSerial(SERIAL_DEBUG_INDEX);
SerialPort HostSerial(SERIAL_HOST_INDEX);
SerialPort FieldbusSerial(SERIAL_FIELDBUS_INDEX);

SerialPort::SerialPort(uint8_t index)
: _index(index), _rxHead(0), _rxTail(0), _txHead(0), _txTail(0)
{
}

void SerialPort::Begin(uint32_t baudRate)
{
    SerialHardwareBegin(_index, baudRate);
}

uint8_t SerialPort::Available() const
{
    return (uint8_t)((_rxHead - _rxTail) & RX_MASK);
}

int16_t SerialPort::Read()
{
    uint8_t tail = _rxTail;
    if(tail == _rxHead)
    {
        return -1;
    }

    uint8_t data = _rxBuffer[tail];
    _rxTail = (uint8_t)((tail + 1) & RX_MASK);
    return data;
}

uint8_t SerialPort::WriteSpace() const
{
    return (uint8_t)((_txTail - _txHead - 1) & TX_MASK);
}

bool SerialPort::Write(uint8_t data)
{
    if(WriteSpace() == 0)
    {
        return false;
    }

    Push(data);
    SerialHardwareStartTransmit(_index);
    return true;
}

bool SerialPort::Write(const char *data, uint8_t length)
{
    if(length > WriteSpace())
    {
        return false;
    }

    uint8_t head = _txHead;
    uint8_t first = (uint8_t)(SERIAL_TX_BUFFER_SIZE - head);
    if(first > length)
    {
        first = length;
    }

    memcpy(_txBuffer + head, data, first);
    memcpy(_txBuffer, data + first, (size_t)(length - first));
    __asm__ __volatile__("" ::: "memory");
    _txHead = (uint8_t)((head + length) & TX_MASK);

    SerialHardwareStartTransmit(_index);
    return true;
}

void SerialPort::OnReceive(uint8_t data)
{
    uint8_t head = _rxHead;
    uint8_t next = (uint8_t)((head + 1) & RX_MASK);
    if(next == _rxTail)
    {
        return;
    }

    _rxBuffer[head] = data;
    _rxHead = next;
}

bool SerialPort::TakeTransmit(uint8_t *data)
{
    uint8_t tail = _txTail;
    if(tail == _txHead)
    {
        return false;
    }

    *data = _txBuffer[tail];
    _txTail = (uint8_t)((tail + 1) & TX_MASK);
    return true;
}

void SerialPort::Push(uint8_t data)
{
    uint8_t head = _txHead;
    _txBuffer[head] = data;
    _txHead = (uint8_t)((head + 1) & TX_MASK);
}
