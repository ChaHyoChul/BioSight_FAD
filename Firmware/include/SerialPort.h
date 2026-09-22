#ifndef SERIALPORT_H_
#define SERIALPORT_H_

#include <stdint.h>

#define SERIAL_RX_BUFFER_SIZE (64)
#define SERIAL_TX_BUFFER_SIZE (128)

#define SERIAL_DEBUG_INDEX (0)
#define SERIAL_HOST_INDEX (1)
#define SERIAL_FIELDBUS_INDEX (3)

class SerialPort
{
private:
    const uint8_t _index;
    volatile uint8_t _rxHead;
    volatile uint8_t _rxTail;
    volatile uint8_t _txHead;
    volatile uint8_t _txTail;
    uint8_t _rxBuffer[SERIAL_RX_BUFFER_SIZE];
    uint8_t _txBuffer[SERIAL_TX_BUFFER_SIZE];

public:
    explicit SerialPort(uint8_t index);

public:
    void Begin(uint32_t baudRate);
    uint8_t Available() const;
    int16_t Read();
    uint8_t WriteSpace() const;
    bool Write(uint8_t data);
    bool Write(const char *data, uint8_t length);

    void OnReceive(uint8_t data);
    bool TakeTransmit(uint8_t *data);

private:
    void Push(uint8_t data);
};

extern SerialPort DebugSerial;
extern SerialPort HostSerial;
extern SerialPort FieldbusSerial;

#endif
