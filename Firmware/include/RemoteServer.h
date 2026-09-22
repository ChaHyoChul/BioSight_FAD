#ifndef REMOTESERVER_H_
#define REMOTESERVER_H_

#include <stdint.h>
#include "GlobalDefinition.h"
#include "SerialPort.h"

typedef struct
{
    char Command[REMOTE_COMMAND_BUFFER_SIZE];
    char Arguments[REMOTE_ARGUMENT_COUNT][REMOTE_ARGUMENT_BUFFER_SIZE];
    uint8_t ArgumentLengths[REMOTE_ARGUMENT_COUNT];
    uint8_t ArgumentCount;
    const char *Error;
} RemoteRequest;

class RemoteServer
{
private:
    SerialPort &_serial;
    char _buffer[REMOTE_RECEIVE_BUFFER_SIZE] = {};
    uint8_t _bufferIndex = 0;
    int8_t _stxIndex = -1;
    int8_t _etxIndex = -1;

public:
    explicit RemoteServer(SerialPort &serial);

public:
    void Process();

private:
    [[gnu::noinline]] void Receive();
    [[gnu::noinline]] void HandleFrame();
    void ClearReceiveBuffer();

    bool ParseRequest(RemoteRequest &request, int stxIndex, int etxIndex) const;
    char *BuildResponse(const RemoteRequest &request, char *response, const char *limit) const;
    char *WriteStatus(const char *command, char *response, const char *limit, bool includeAnalogOutputs) const;

    const char *SetDigitalOutput(const RemoteRequest &request) const;
    const char *ToggleDigitalOutput(const RemoteRequest &request) const;
    const char *SetAnalogOutput(const RemoteRequest &request) const;
};

#endif
