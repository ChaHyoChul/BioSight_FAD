#ifndef KISANMODULE_H_
#define KISANMODULE_H_

#include <stdint.h>
#include "GlobalDefinition.h"
#include "SerialPort.h"
#include "Timer.h"

class KiSANModule
{
protected:
    SerialPort &_serial;
    const uint8_t _address;

    uint8_t _state = 0;
    bool _isReceivedStx = false;
    bool _isReceivedEtx = false;
    char _receiveBuffer[KISAN_RECEIVE_BUFFER_SIZE] = {};
    uint8_t _receiveBufferIndex = 0;

    uint8_t _retryCount = 0;
    bool _isConnected = false;

    Timer _receiveTimer;
    Timer _waitTimer;

protected:
    KiSANModule(SerialPort &serial, uint8_t address);

public:
    bool Process();
    bool IsConnected() const;

protected:
    virtual void Send();
    virtual void ProcessReceive();

    void SendFrame(const char *payload);

private:
    bool Receive();
    void ClearReceiveBuffer();
};

#endif
