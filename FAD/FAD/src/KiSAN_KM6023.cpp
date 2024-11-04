#include <Arduino.h>
#include "KiSAN_KM6023.h"
#include "ChecksumCalculator.h"
#include "Global.h"
#include "GlobalDefinition.h"

#define SEND_BUFFER_SIZE (64)
#define RECEIVE_BUFFER_SIZE (64)
#define DATA_COUNT (4)
#define RETRY_COUNT (5)
#define RECEIVE_TIMEOUT_MILLISECONDS (60000)

KiSAN_KM6023::KiSAN_KM6023()
: KiSANModule(1, NULL)
{
	_state = 0;
	_isReceivedStx = false;
	_isReceivedEtx = false;
	_receiveBuffer = new char[RECEIVE_BUFFER_SIZE];
	memset(_receiveBuffer, 0, sizeof(char) * RECEIVE_BUFFER_SIZE);
	_receiveBufferIndex = 0;

	_retryCount = 0;
	_isConnected = false;

	_receiveTimer.Reset();
	_waitTimer.Reset();
}

KiSAN_KM6023::KiSAN_KM6023(unsigned int address, KiSANProtocol* kisanProtocol)
: KiSANModule(address, kisanProtocol)
{
	_state = 0;
	_isReceivedStx = false;
	_isReceivedEtx = false;
	_receiveBuffer = new char[RECEIVE_BUFFER_SIZE];
	memset(_receiveBuffer, 0, sizeof(char) * RECEIVE_BUFFER_SIZE);
	_receiveBufferIndex = 0;

	_retryCount = 0;
	_isConnected = false;

	_receiveTimer.Reset();
}

KiSAN_KM6023::~KiSAN_KM6023()
{
	delete [] _receiveBuffer;
}

void KiSAN_KM6023::Initialize(HardwareSerial *hardwareSerial)
{
	_hardwareSerial = hardwareSerial;
}

bool KiSAN_KM6023::Process()
{
	bool result = false;
	int receiveResult = -1;
	switch(_state)
	{
		case 0:
		Send();
		_receiveTimer.Start(RECEIVE_TIMEOUT_MILLISECONDS);
		_state = 1;
		break;

		case 1:
		receiveResult = Receive();
		if(receiveResult == 1)
		{
			if(_isConnected == false)
			{
				_isConnected = true;
				_retryCount = 0;
			}

			_state = 2;
		}
		else if(receiveResult == 0)
		{
			if(_receiveTimer.IsTimeout())
			{
				if(_retryCount < RETRY_COUNT)
				{
					_retryCount++;
				}
				else
				{
					if(_isConnected == true)
					{
						_isConnected = false;
					}
				}

				_state = 0;
			}
		}
		else
		{
			if(_retryCount < RETRY_COUNT)
			{
				_retryCount++;
			}
			else
			{
				if(_isConnected == true)
				{
					_isConnected = false;
				}
			}

			_state = 0;
		}
		break;

		case 2:
		ProcessReceive();
		_state = 3;
		_waitTimer.Start(500);
		break;
		
		case 3:
		if(_waitTimer.IsTimeout())
		{
			_state = 0;
			_waitTimer.Reset();
			result = true;
		}
		break;

		default:
		_state = 0;
		result = true;
	}
	
	return result;
}

void KiSAN_KM6023::Send()
{
	char sendMessage[SEND_BUFFER_SIZE] = {0};
	char sendMessage2[SEND_BUFFER_SIZE] = {0};
		
	int ch1 = (int)(global._analogInputs[0] * 1000);
	int ch2 = (int)(global._analogInputs[1] * 1000);
	int ch3 = (int)(global._analogInputs[2] * 1000);
	int ch4 = (int)(global._analogInputs[3] * 1000);

	sprintf(sendMessage, "#01S0204,%04X,%04X,%04X,%04X", ch1, ch2, ch3, ch4);
	
	ChecksumCalculator checksumCalculator;
	byte checksum = checksumCalculator.CalculateChecksum(sendMessage, strlen(sendMessage));
	
	sprintf(sendMessage2, "%s%02X%c", sendMessage, checksum, 0x0D);
	
	_hardwareSerial->print(sendMessage2);
	_hardwareSerial->flush();
}

int KiSAN_KM6023::Receive()
{
	int result = 0;

	while(_hardwareSerial->available() > 0)
	{
		int readData = _hardwareSerial->read();
		if(readData == -1)
		{
			break;
		}

		if(!_isReceivedStx)
		{
			if(readData == '*')
			{
				_isReceivedStx = true;
				continue;
			}
		}
		else if(!_isReceivedEtx)
		{
			if(readData == 0x0D)
			{
				_isReceivedEtx = true;
				break;
			}
			else
			{
				_receiveBuffer[_receiveBufferIndex] = (char)readData;
				_receiveBufferIndex++;
			}
		}
	}

	if(_isReceivedStx && _isReceivedEtx)
	{
		byte checksum = (_receiveBuffer[_receiveBufferIndex - 2] << 4) + (_receiveBuffer[_receiveBufferIndex - 1]);

		_receiveBuffer[--_receiveBufferIndex] = 0;
		_receiveBuffer[--_receiveBufferIndex] = 0;
		
		ChecksumCalculator checksumCalculator;
		byte calculatedChecksum = checksumCalculator.CalculateChecksum(_receiveBuffer, _receiveBufferIndex);

		//if(checksum != calculatedChecksum)
		//{
		//result = -1;
		//}
		//else
		//{
		result = 1;
		//}

		_isReceivedStx = false;
		_isReceivedEtx = false;
	}

	return result;
}

void KiSAN_KM6023::ProcessReceive()
{
	char* token = strtok(_receiveBuffer, ",");

	int i = 0;
	while(token != NULL)
	{
		token = strtok(NULL, ",");
		
		if(token == NULL || strcmp(token, "") == 0)
		{
			break;
		}

		unsigned long data = strtoul(token, NULL, 16);
		global._analogOutputsKisan[i] = (data / 1000.0);
		
		i++;
	}
	
	memset(_receiveBuffer, 0, sizeof(int) * RECEIVE_BUFFER_SIZE);
	_receiveBufferIndex = 0;
}
