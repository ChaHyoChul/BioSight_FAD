#ifndef KISAN_KM6015_H_
#define KISAN_KM6015_H_

#include "KiSANModule.h"
#include "Timer.h"
#include <Arduino.h>

class KiSAN_KM6015 : public KiSANModule
{
private:
    HardwareSerial *_hardwareSerial;
    int _state;
    bool _isReceivedStx;
    bool _isReceivedEtx;
    char *_receiveBuffer;
    int _receiveBufferIndex;

    int _retryCount;
    bool _isConnected;

    Timer _receiveTimer;
	Timer _waitTimer;

public:
    KiSAN_KM6015();
    KiSAN_KM6015(unsigned int address, KiSANProtocol* kisanProtocol);
    ~KiSAN_KM6015();

public:
    void Initialize(HardwareSerial *hardwareSerial);
    virtual bool Process();

private:
    void Send();
    int Receive();
    void ProcessReceive();
};

#endif
