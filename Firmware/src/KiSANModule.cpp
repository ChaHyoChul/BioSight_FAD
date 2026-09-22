#include "KiSANModule.h"
#include "ChecksumCalculator.h"
#include "TextFormat.h"

#define KISAN_STATE_SEND (0)
#define KISAN_STATE_RECEIVE (1)
#define KISAN_STATE_APPLY (2)
#define KISAN_STATE_WAIT (3)

#define KISAN_REQUEST_STX ('#')
#define KISAN_STX ('*')
#define KISAN_ETX (0x0D)
#define KISAN_CHECKSUM_LENGTH (2)
#define KISAN_ADDRESS_DIGITS (2)

KiSANModule::KiSANModule(SerialPort &serial, uint8_t address)
: _serial(serial), _address(address)
{
}

bool KiSANModule::IsConnected() const
{
    return _isConnected;
}

bool KiSANModule::Process()
{
    switch(_state)
    {
    case KISAN_STATE_SEND:
        ClearReceiveBuffer();
        Send();
        _receiveTimer.Start(KISAN_RECEIVE_TIMEOUT_MILLISECONDS);
        _state = KISAN_STATE_RECEIVE;
        break;

    case KISAN_STATE_RECEIVE:
        if(Receive())
        {
            _isConnected = true;
            _retryCount = 0;
            _state = KISAN_STATE_APPLY;
            break;
        }

        if(_receiveTimer.IsTimeout())
        {
            if(_retryCount < KISAN_RETRY_COUNT)
            {
                _retryCount++;
            }
            else
            {
                _isConnected = false;
            }

            _state = KISAN_STATE_SEND;
            return true;
        }
        break;

    case KISAN_STATE_APPLY:
        ProcessReceive();
        ClearReceiveBuffer();
        _waitTimer.Start(KISAN_POLL_INTERVAL_MILLISECONDS);
        _state = KISAN_STATE_WAIT;
        break;

    default:
        if(_waitTimer.IsTimeout())
        {
            _state = KISAN_STATE_SEND;
            return true;
        }
        break;
    }

    return false;
}

void KiSANModule::Send()
{
}

void KiSANModule::ProcessReceive()
{
}

void KiSANModule::SendFrame(const char *payload)
{
    char frame[KISAN_SEND_BUFFER_SIZE];
    const char *limit = frame + KISAN_SEND_BUFFER_SIZE - KISAN_CHECKSUM_LENGTH - 1;

    char *out = frame;
    *out++ = KISAN_REQUEST_STX;
    out = WriteDecimal(out, _address, KISAN_ADDRESS_DIGITS);
    out = WriteText(out, limit, payload);
    if(out >= limit)
    {
        return;
    }

    out = WriteHex(out, CalculateChecksum(frame, (int)(out - frame)), KISAN_CHECKSUM_LENGTH);
    *out++ = KISAN_ETX;
    _serial.Write(frame, (uint8_t)(out - frame));
}

bool KiSANModule::Receive()
{
    int16_t readData;
    while((readData = _serial.Read()) >= 0)
    {
        if(!_isReceivedStx)
        {
            if(readData == KISAN_STX)
            {
                _isReceivedStx = true;
            }
            continue;
        }

        if(readData == KISAN_ETX)
        {
            _isReceivedEtx = true;
            break;
        }

        if(_receiveBufferIndex >= (KISAN_RECEIVE_BUFFER_SIZE - 1))
        {
            ClearReceiveBuffer();
            continue;
        }

        _receiveBuffer[_receiveBufferIndex] = (char)readData;
        _receiveBufferIndex++;
    }

    if(!_isReceivedEtx)
    {
        return false;
    }

    _isReceivedStx = false;
    _isReceivedEtx = false;

    if(_receiveBufferIndex < KISAN_CHECKSUM_LENGTH)
    {
        ClearReceiveBuffer();
        return false;
    }

    _receiveBufferIndex = (uint8_t)(_receiveBufferIndex - KISAN_CHECKSUM_LENGTH);
    _receiveBuffer[_receiveBufferIndex] = 0;
    _receiveBuffer[_receiveBufferIndex + 1] = 0;

    return true;
}

void KiSANModule::ClearReceiveBuffer()
{
    _receiveBufferIndex = 0;
    _isReceivedStx = false;
    _isReceivedEtx = false;
}
