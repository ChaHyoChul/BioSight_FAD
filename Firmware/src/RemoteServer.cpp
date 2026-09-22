#include "RemoteServer.h"
#include "Board.h"
#include "Global.h"
#include "TextFormat.h"
#include <string.h>

#define REMOTE_STX (0x02)
#define REMOTE_ETX (0x03)
#define REMOTE_FRAME_OVERHEAD (2)

#define ERROR_COMMAND_DATA_BUFFER_OVERFLOW ("E0001")
#define ERROR_ARGUMENT_COUNT_BUFFER_OVERFLOW ("E0002")
#define ERROR_ARGUMENT_DATA_BUFFER_OVERFLOW ("E0003")
#define ERROR_ARGUMENT_NOT_FOUND ("E0004")
#define ERROR_NOT_SUPPORTED_COMMAND ("E0005")
#define ERROR_INVALID_ARGUMENT_COUNT ("E0006")
#define ERROR_INVALID_ARGUMENT_FORMAT ("E0007")
#define ERROR_INVALID_ARGUMENT_RANGE ("E0008")
#define ERROR_INVALID_CONTROL_MODE ("E0009")

#define TOTAL_DIGITAL_OUTPUT_COUNT (DIGITAL_OUTPUT_COUNT + KISAN_DIGITAL_OUTPUT_COUNT)
#define STATUS_HEADER_LENGTH (14)
#define STATUS_NUMBER_MAX_LENGTH (10)
#define ARGUMENT_SATURATION (UINT16_MAX)

static_assert(REMOTE_RESPONSE_BUFFER_SIZE + REMOTE_FRAME_OVERHEAD < SERIAL_TX_BUFFER_SIZE, "A full response frame must fit in the transmit buffer");
static_assert(KISAN_ANALOG_OUTPUT_MAX_VALUE < ARGUMENT_SATURATION / 10 * 10, "Saturated arguments must stay out of every valid range");

static bool ParseArgument(const char *text, uint8_t length, uint16_t &value)
{
    if(length == 0)
    {
        return false;
    }

    value = 0;
    for(uint8_t i = 0; i < length; i++)
    {
        uint8_t digit = (uint8_t)(text[i] - '0');
        if(digit > 9)
        {
            return false;
        }

        value = value < ARGUMENT_SATURATION / 10 ? (uint16_t)(value * 10 + digit) : ARGUMENT_SATURATION;
    }

    return true;
}

[[gnu::noinline]] static const char *ParseArguments(const RemoteRequest &request, uint16_t *values, uint8_t count)
{
    if(request.ArgumentCount != count)
    {
        return ERROR_INVALID_ARGUMENT_COUNT;
    }

    for(uint8_t i = 0; i < count; i++)
    {
        if(!ParseArgument(request.Arguments[i], request.ArgumentLengths[i], values[i]))
        {
            return ERROR_INVALID_ARGUMENT_FORMAT;
        }
    }

    return NULL;
}

static char *WriteValues(char *out, const char *limit, const float *values, uint8_t count)
{
    for(uint8_t i = 0; i < count && limit - out >= STATUS_NUMBER_MAX_LENGTH; i++)
    {
        *out++ = ',';
        out = WriteHundredths(out, values[i]);
    }
    return out;
}

static char *WriteReply(char *out, const char *limit, const char *command, const char *detail)
{
    out = WriteText(out, limit, command);
    if(detail != NULL && out < limit)
    {
        *out++ = ' ';
        out = WriteText(out, limit, detail);
    }
    return out;
}

RemoteServer::RemoteServer(SerialPort &serial)
: _serial(serial)
{
}

void RemoteServer::Process()
{
    if(_etxIndex < 0)
    {
        if(_serial.Available() == 0)
        {
            return;
        }

        Receive();
        if(_etxIndex < 0)
        {
            return;
        }
    }

    if(_serial.WriteSpace() < REMOTE_RESPONSE_BUFFER_SIZE + REMOTE_FRAME_OVERHEAD)
    {
        return;
    }

    HandleFrame();
}

void RemoteServer::HandleFrame()
{
    RemoteRequest request;
    char response[REMOTE_RESPONSE_BUFFER_SIZE + REMOTE_FRAME_OVERHEAD];
    response[0] = REMOTE_STX;

    char *body = response + 1;
    const char *limit = body + REMOTE_RESPONSE_BUFFER_SIZE - 1;
    char *end = ParseRequest(request, _stxIndex, _etxIndex) ? BuildResponse(request, body, limit) : WriteReply(body, limit, request.Command, request.Error);
    *end++ = REMOTE_ETX;

    _serial.Write(response, (uint8_t)(end - response));
    ClearReceiveBuffer();
}

void RemoteServer::Receive()
{
    while(_etxIndex < 0)
    {
        int16_t readData = _serial.Read();
        if(readData < 0)
        {
            return;
        }

        if(readData == REMOTE_STX)
        {
            ClearReceiveBuffer();
            _stxIndex = 0;
        }
        else if(_stxIndex < 0)
        {
            continue;
        }
        else if(_bufferIndex >= REMOTE_RECEIVE_BUFFER_SIZE)
        {
            ClearReceiveBuffer();
            continue;
        }

        if(readData == REMOTE_ETX)
        {
            _etxIndex = (int8_t)_bufferIndex;
        }

        _buffer[_bufferIndex] = (char)readData;
        _bufferIndex++;
    }
}

void RemoteServer::ClearReceiveBuffer()
{
    _bufferIndex = 0;
    _stxIndex = -1;
    _etxIndex = -1;
}

bool RemoteServer::ParseRequest(RemoteRequest &request, int stxIndex, int etxIndex) const
{
    memset(request.Command, 0, sizeof(request.Command));
    memset(request.ArgumentLengths, 0, sizeof(request.ArgumentLengths));
    request.ArgumentCount = 0;

    uint8_t commandLength = 0;
    uint8_t argumentIndex = 0;
    bool isReceivedCommand = false;

    for(int i = stxIndex + 1; i < etxIndex; i++)
    {
        char data = _buffer[i];

        if(!isReceivedCommand)
        {
            if(data == ' ')
            {
                isReceivedCommand = true;
                continue;
            }

            if(commandLength >= (REMOTE_COMMAND_BUFFER_SIZE - 1))
            {
                request.Error = ERROR_COMMAND_DATA_BUFFER_OVERFLOW;
                return false;
            }

            request.Command[commandLength] = data;
            commandLength++;
            continue;
        }

        if(data == ',')
        {
            argumentIndex++;

            if(argumentIndex >= REMOTE_ARGUMENT_COUNT)
            {
                request.Error = ERROR_ARGUMENT_COUNT_BUFFER_OVERFLOW;
                return false;
            }

            continue;
        }

        if(request.ArgumentLengths[argumentIndex] >= (REMOTE_ARGUMENT_BUFFER_SIZE - 1))
        {
            request.Error = ERROR_ARGUMENT_DATA_BUFFER_OVERFLOW;
            return false;
        }

        request.Arguments[argumentIndex][request.ArgumentLengths[argumentIndex]] = data;
        request.ArgumentLengths[argumentIndex]++;
    }

    if(!isReceivedCommand)
    {
        request.ArgumentCount = 0;
        return true;
    }

    if(argumentIndex == 0)
    {
        if(request.ArgumentLengths[0] == 0)
        {
            request.Error = ERROR_ARGUMENT_NOT_FOUND;
            return false;
        }

        request.ArgumentCount = 1;
        return true;
    }

    request.ArgumentCount = argumentIndex;
    if(request.ArgumentLengths[argumentIndex] > 0)
    {
        request.ArgumentCount = (uint8_t)(argumentIndex + 1);
    }

    return true;
}

char *RemoteServer::BuildResponse(const RemoteRequest &request, char *response, const char *limit) const
{
    if(strcmp(request.Command, "GVER") == 0)
    {
        return WriteReply(response, limit, request.Command, VERSION);
    }

    if(strcmp(request.Command, "OXYG") == 0)
    {
        char *out = WriteText(response, limit, request.Command);
        *out++ = ' ';
        out = WriteDecimal(out, (int16_t)(global._oxygenConcentration + 0.5f), 1);
        *out++ = ' ';
        out = WriteDecimal(out, (int16_t)(global._analogInputsKisan[OXYGEN_SENSOR_INPUT_INDEX] + 0.5f), 1);
        *out++ = ' ';
        return out;
    }

    bool isDetailedStatus = strcmp(request.Command, "GDST") == 0;
    if(isDetailedStatus || strcmp(request.Command, "GSTA") == 0)
    {
        return WriteStatus(request.Command, response, limit, isDetailedStatus);
    }

    const char *error = ERROR_NOT_SUPPORTED_COMMAND;

    if(strcmp(request.Command, "SSDO") == 0)
    {
        error = SetDigitalOutput(request);
    }
    else if(strcmp(request.Command, "TSDO") == 0)
    {
        error = ToggleDigitalOutput(request);
    }
    else if(strcmp(request.Command, "SSAO") == 0)
    {
        error = SetAnalogOutput(request);
    }

    return WriteReply(response, limit, request.Command, error);
}

char *RemoteServer::WriteStatus(const char *command, char *response, const char *limit, bool includeAnalogOutputs) const
{
    char *out = WriteText(response, limit, command);

    if(limit - out >= STATUS_HEADER_LENGTH)
    {
        uint16_t kisanOutputs = PackFlags(global._kisanOutputs, KISAN_DIGITAL_OUTPUT_COUNT);
        uint16_t outputs = (uint16_t)(PackFlags(global._digitalOutputs, DIGITAL_OUTPUT_COUNT) | ((unsigned int)kisanOutputs << DIGITAL_OUTPUT_COUNT));

        *out++ = ' ';
        *out++ = (char)('0' + (int)global._systemMode);
        *out++ = ',';
        *out++ = (char)('0' + (int)global._controlMode);
        *out++ = ',';
        out = WriteHex(out, PackFlags(global._digitalInputs, DIGITAL_INPUT_COUNT), 4);
        *out++ = ',';
        out = WriteHex(out, outputs, 4);
    }

    out = WriteValues(out, limit, global._analogInputs, ANALOG_INPUT_COUNT);
    out = WriteValues(out, limit, global._analogInputsKisan, KISAN_ANALOG_INPUT_COUNT);
    if(includeAnalogOutputs)
    {
        out = WriteValues(out, limit, global._analogOutputsKisan, KISAN_ANALOG_OUTPUT_COUNT);
    }

    return out;
}

const char *RemoteServer::SetDigitalOutput(const RemoteRequest &request) const
{
    if(global._controlMode != ControlMode::CM_Auto)
    {
        return ERROR_INVALID_CONTROL_MODE;
    }

    uint16_t values[2];
    const char *error = ParseArguments(request, values, (uint8_t)(sizeof(values) / sizeof(values[0])));
    if(error != NULL)
    {
        return error;
    }

    if(values[0] >= TOTAL_DIGITAL_OUTPUT_COUNT || values[1] > 1)
    {
        return ERROR_INVALID_ARGUMENT_RANGE;
    }

    uint8_t outputIndex = (uint8_t)values[0];
    bool targetState = values[1] == 1;

    if(outputIndex < DIGITAL_OUTPUT_COUNT)
    {
        Board::WriteDigitalOutput(outputIndex, targetState);
        return NULL;
    }

    global._kisanOutputs[outputIndex - DIGITAL_OUTPUT_COUNT] = targetState;
    return NULL;
}

const char *RemoteServer::ToggleDigitalOutput(const RemoteRequest &request) const
{
    if(global._controlMode != ControlMode::CM_Auto)
    {
        return ERROR_INVALID_CONTROL_MODE;
    }

    uint16_t values[1];
    const char *error = ParseArguments(request, values, (uint8_t)(sizeof(values) / sizeof(values[0])));
    if(error != NULL)
    {
        return error;
    }

    if(values[0] >= TOTAL_DIGITAL_OUTPUT_COUNT)
    {
        return ERROR_INVALID_ARGUMENT_RANGE;
    }

    uint8_t outputIndex = (uint8_t)values[0];

    if(outputIndex < DIGITAL_OUTPUT_COUNT)
    {
        Board::WriteDigitalOutput(outputIndex, !global._digitalOutputs[outputIndex]);
        return NULL;
    }

    uint8_t kisanIndex = (uint8_t)(outputIndex - DIGITAL_OUTPUT_COUNT);
    global._kisanOutputs[kisanIndex] = !global._kisanOutputs[kisanIndex];
    return NULL;
}

const char *RemoteServer::SetAnalogOutput(const RemoteRequest &request) const
{
    uint16_t values[2];
    const char *error = ParseArguments(request, values, (uint8_t)(sizeof(values) / sizeof(values[0])));
    if(error != NULL)
    {
        return error;
    }

    if(values[0] >= KISAN_ANALOG_OUTPUT_COUNT || values[1] > KISAN_ANALOG_OUTPUT_MAX_VALUE)
    {
        return ERROR_INVALID_ARGUMENT_RANGE;
    }

    global._analogOutputsKisanTargets[values[0]] = (float)values[1];
    return NULL;
}
