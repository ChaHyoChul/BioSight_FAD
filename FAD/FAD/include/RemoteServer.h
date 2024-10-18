#ifndef REMOTESERVER_H_
#define REMOTESERVER_H_

#include <Arduino.h>

class RemoteServer
{
private:
    HardwareSerial *_serial;
    char *_buffer;
    int _bufferIndex;

public:
    RemoteServer();
    ~RemoteServer();

public:
    void Initialize(HardwareSerial *serial, unsigned int baudRate);
    void Process();

    void Receive();
    int FindCharInBuffer(char ch);

private:
    void ClearReceiveBuffer();
};

#endif
