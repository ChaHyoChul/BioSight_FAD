#include "RemoteServer.h"
#include "GlobalDefinition.h"
#include "Global.h"

#define RECEIVE_BUFFER_SIZE (32)
#define COMMAND_DATA_BUFFER_SIZE (8)
#define ARGUMENT_COUNT_BUFFER_SIZE (8)
#define ARGUMENT_DATA_BUFFER_SIZE (8)
#define RESPONSE_DATA_BUFFER_SIZE (128)

#define ERROR_COMMAND_DATA_BUFFER_OVERFLOW ("E0001")
#define ERROR_ARGUMENT_COUNT_BUFFER_OVERFLOW ("E0002")
#define ERROR_ARGUMENT_DATA_BUFFER_OVERFLOW ("E0003")
#define ERROR_ARGUMENT_NOT_FOUND ("E0004")
#define ERROR_NOT_SUPPORTED_COMMAND ("E0005")
#define ERROR_INVALID_ARGUMENT_COUNT ("E0006")
#define ERROR_INVALID_ARGUMENT_FORMAT ("E0007")
#define ERROR_INVALID_ARGUMENT_RANGE ("E0008")
#define ERROR_INVALID_CONTROL_MODE ("E0009")

RemoteServer::RemoteServer()
{
    _buffer = new char[RECEIVE_BUFFER_SIZE];
    ClearReceiveBuffer();
}

RemoteServer::~RemoteServer()
{
    delete [] _buffer;
}

void RemoteServer::Initialize(HardwareSerial *serial, unsigned int baudRate)
{
    _serial = serial;
    _serial->begin(baudRate);
}

void RemoteServer::Process()
{
    Receive();

    int stxIndex = FindCharInBuffer(0x02);
    if(stxIndex == -1)
    {
        return;
    }
    
    int etxIndex = FindCharInBuffer(0x03);
    if(etxIndex == -1)
    {
        return;
    }

    char command[COMMAND_DATA_BUFFER_SIZE] = {0};
    int commandDataIndex = 0;

    char arguments[ARGUMENT_COUNT_BUFFER_SIZE][ARGUMENT_DATA_BUFFER_SIZE] = {0};
    int argumentIndex = 0;
    int argumentDataIndexs[ARGUMENT_COUNT_BUFFER_SIZE] = {0};

    bool isReceivedCommand = false;
    bool isError = false;

    for(int i = stxIndex + 1; i < etxIndex; i++)
    {
        if(!isReceivedCommand)
        {
            if(_buffer[i] == ' ')
            {
                isReceivedCommand = true;
            }
            else
            {
                if(commandDataIndex >= COMMAND_DATA_BUFFER_SIZE)
                {
                    command[commandDataIndex - 1] = '\0';
                    memset(arguments[0], 0, sizeof(char) * ARGUMENT_DATA_BUFFER_SIZE);
                    strcpy(arguments[0], ERROR_COMMAND_DATA_BUFFER_OVERFLOW);
                    argumentDataIndexs[0] = strlen(ERROR_COMMAND_DATA_BUFFER_OVERFLOW);
                    argumentIndex = 1;
                    isError = true;
                    break;
                }

                command[commandDataIndex] = _buffer[i];
                commandDataIndex++;
            }
        }
        else
        {
            if(_buffer[i] == ',')
            {
                argumentIndex++;

                if(argumentIndex >= ARGUMENT_COUNT_BUFFER_SIZE)
                {
                    memset(arguments[0], 0, sizeof(char) * ARGUMENT_DATA_BUFFER_SIZE);
                    strcpy(arguments[0], ERROR_ARGUMENT_COUNT_BUFFER_OVERFLOW);
                    argumentDataIndexs[0] = strlen(ERROR_ARGUMENT_COUNT_BUFFER_OVERFLOW);
                    argumentIndex = 1;
                    isError = true;
                    break;
                }
            }
            else
            {
                if(argumentDataIndexs[argumentIndex] >= ARGUMENT_DATA_BUFFER_SIZE)
                {
                    memset(arguments[0], 0, sizeof(char) * ARGUMENT_DATA_BUFFER_SIZE);
                    strcpy(arguments[0], ERROR_ARGUMENT_DATA_BUFFER_OVERFLOW);
                    argumentDataIndexs[0] = strlen(ERROR_ARGUMENT_DATA_BUFFER_OVERFLOW);
                    argumentIndex = 1;
                    isError = true;
                    break;
                }

                arguments[argumentIndex][argumentDataIndexs[argumentIndex]] = _buffer[i];
                argumentDataIndexs[argumentIndex]++;
            }
        }
    }

    if(!isError)
    {
        if(isReceivedCommand)
        {
            if(argumentIndex == 0)
            {
                if(arguments[0][0] != 0)
                {
                    argumentIndex = 1;
                }
                else
                {
                    memset(arguments[0], 0, sizeof(char) * ARGUMENT_DATA_BUFFER_SIZE);
                    strcpy(arguments[0], ERROR_ARGUMENT_NOT_FOUND);
                    argumentDataIndexs[0] = strlen(ERROR_ARGUMENT_NOT_FOUND);
                    argumentIndex = 1;
                    isError = true;
                }
            }
            else
            {
                if(arguments[argumentIndex][0] != 0)
                {
                    argumentIndex++;
                }
            }
        }
    }
    
    char response[RESPONSE_DATA_BUFFER_SIZE] = {0};

    if(isError)
    {
        sprintf(response, "%s %s", command, arguments[0]);
    }
    else
    {
        if(strcmp(command, "GVER") == 0)
        {
            sprintf(response, "%s %s", command, VERSION);
        }
        else if(strcmp(command, "GSTA") == 0)
        {
            unsigned short inputs = 0;
            unsigned short outputs = 0;

            for(int i = 0; i < DIGITAL_INPUT_COUNT; i++)
            {
                inputs |= (global._digitalInputs[i] << i);
            }

            for(int i = 0; i < DIGITAL_OUTPUT_COUNT; i++)
            {
                outputs |= (global._digitalOutputs[i] << i);
            }
			
			for(int i=0; i<KISAN_DIGITAL_OUTPUT_COUNT; i++)
			{
				outputs |= (global._kisanOutputs[i] << (i + DIGITAL_OUTPUT_COUNT));
			}

            sprintf(response, "%s %d,%d,%04X,%04X",
                command,
                global._systemMode, global._controlMode,
                inputs, outputs);

            const int tempBufferCount = 5;
            char temp[tempBufferCount] = {0};
            for(int i = 0; i < ANALOG_INPUT_COUNT; i++)
            {
                memset(temp, 0, sizeof(char) * tempBufferCount);
                dtostrf(global._analogInputs[i], 2, 2, temp);
                strcat(response, ",");
                strcat(response, temp);
            }

            for(int i = 0; i < KISAN_ANALOG_INPUT_COUNT; i++)
            {
                memset(temp, 0, sizeof(char) * tempBufferCount);
                dtostrf(global._analogInputsKisan[i], 2, 2, temp);
                strcat(response, ",");
                strcat(response, temp);
            }
			//
			//for(int i = 0; i < KISAN_ANALOG_OUTPUT_COUNT; i++)
			//{
				//memset(temp, 0, sizeof(char) * tempBufferCount);
				//dtostrf(global._analogOutputsKisan[i], 2, 2, temp);
				//strcat(response, ",");
				//strcat(response, temp);
			//}
			//
			//for(int i = 0; i < KISAN_ANALOG_OUTPUT_COUNT; i++)
			//{
				//memset(temp, 0, sizeof(char) * tempBufferCount);
				//dtostrf(global._analogOutputsKisanTargets[i], 2, 2, temp);
				//strcat(response, ",");
				//strcat(response, temp);
			//}
        }
		else if(strcmp(command, "GDST") == 0)
		{
			unsigned short inputs = 0;
			unsigned short outputs = 0;

			for(int i = 0; i < DIGITAL_INPUT_COUNT; i++)
			{
				inputs |= (global._digitalInputs[i] << i);
			}

			for(int i = 0; i < DIGITAL_OUTPUT_COUNT; i++)
			{
				outputs |= (global._digitalOutputs[i] << i);
			}
			
			for(int i=0; i<KISAN_DIGITAL_OUTPUT_COUNT; i++)
			{
				outputs |= (global._kisanOutputs[i] << (i + DIGITAL_OUTPUT_COUNT));
			}

			sprintf(response, "%s %d,%d,%04X,%04X",
			command,
			global._systemMode, global._controlMode,
			inputs, outputs);

			const int tempBufferCount = 5;
			char temp[tempBufferCount] = {0};
			for(int i = 0; i < ANALOG_INPUT_COUNT; i++)
			{
				memset(temp, 0, sizeof(char) * tempBufferCount);
				dtostrf(global._analogInputs[i], 2, 2, temp);
				strcat(response, ",");
				strcat(response, temp);
			}

			for(int i = 0; i < KISAN_ANALOG_INPUT_COUNT; i++)
			{
				memset(temp, 0, sizeof(char) * tempBufferCount);
				dtostrf(global._analogInputsKisan[i], 2, 2, temp);
				strcat(response, ",");
				strcat(response, temp);
			}
			
			for(int i = 0; i < KISAN_ANALOG_OUTPUT_COUNT; i++)
			{
				memset(temp, 0, sizeof(char) * tempBufferCount);
				dtostrf(global._analogOutputsKisan[i], 2, 2, temp);
				strcat(response, ",");
				strcat(response, temp);
			}
		}
        else if (strcmp(command, "SSDO") == 0)
        {
            if(global._controlMode == ControlMode::CM_Auto)
            {
                bool isPassedArgumentCheck = true;
                if(argumentIndex != 2)
                {
                    sprintf(response, "%s %s", command, ERROR_INVALID_ARGUMENT_COUNT);
                    isPassedArgumentCheck = false;
                }

                for(int i = 0; i < argumentIndex; i++)
                {
                    for(int j = 0; j < argumentDataIndexs[i]; j++)
                    {
                        if(arguments[i][j] < '0' || arguments[i][j] > '9')
                        {
                            sprintf(response, "%s %s", command, ERROR_INVALID_ARGUMENT_FORMAT);
                            isPassedArgumentCheck = false;
                            break;
                        }
                    }
                }

                int outputIndex = 0;
                int outputTargetState = 0;

                if(isPassedArgumentCheck)
                {
                    outputIndex = atoi(arguments[0]);
                    outputTargetState = atoi(arguments[1]);

                    if (outputIndex < 0 || outputIndex >= (DIGITAL_OUTPUT_COUNT + KISAN_DIGITAL_OUTPUT_COUNT) ||
                    outputTargetState < 0 || outputTargetState > 1)
                    {
                        sprintf(response, "%s %s", command, ERROR_INVALID_ARGUMENT_RANGE);
                        isPassedArgumentCheck = false;
                    }
                }

                if(isPassedArgumentCheck)
                {
					if(outputIndex < DIGITAL_OUTPUT_COUNT)
					{
						int pinNumber = DIGITAL_OUTPUT_FIRST_PIN + outputIndex;
						int targetState = outputTargetState == 0 ? LOW : HIGH;
						
						digitalWrite(pinNumber, targetState);
						
						// NOTE: 파스퇴르 연구소 전용
						//if(outputIndex == 0)
						//{
							//digitalWrite(pinNumber + 5, targetState);
						//}
					}
					else
					{
						int kisanOutputIndex = outputIndex - DIGITAL_OUTPUT_COUNT;
						bool targetState = outputTargetState == 1 ? true : false;
						
						global._kisanOutputs[kisanOutputIndex] = targetState;
					}
					
					sprintf(response, "%s", command);
                }
            }
            else
            {
                sprintf(response, "%s %s", command, ERROR_INVALID_CONTROL_MODE);
            }
        }
        else if(strcmp(command, "TSDO") == 0)
        {
            if(global._controlMode == ControlMode::CM_Auto)
            {
                bool isPassedArgumentCheck = true;
                if(argumentIndex != 1)
                {
                    sprintf(response, "%s %s", command, ERROR_INVALID_ARGUMENT_COUNT);
                    isPassedArgumentCheck = false;
                }

                for(int i = 0; i < argumentIndex; i++)
                {
                    if(arguments[0][i] < '0' || arguments[0][i] > '9')
                    {
                        sprintf(response, "%s %s", command, ERROR_INVALID_ARGUMENT_FORMAT);
                        isPassedArgumentCheck = false;
                        break;
                    }
                }

                int outputIndex = 0;

                if(isPassedArgumentCheck)
                {
                    outputIndex = atoi(arguments[0]);

                    if (outputIndex < 0 || outputIndex >= (DIGITAL_OUTPUT_COUNT + KISAN_DIGITAL_OUTPUT_COUNT))
                    {
                        sprintf(response, "%s %s", command, ERROR_INVALID_ARGUMENT_RANGE);
                        isPassedArgumentCheck = false;
                    }
                }

                if(isPassedArgumentCheck)
                {
					if(outputIndex < DIGITAL_OUTPUT_COUNT)
					{
						int pinNumber = DIGITAL_OUTPUT_FIRST_PIN + outputIndex;
						digitalWrite(pinNumber, global._digitalOutputs[outputIndex] ? LOW : HIGH);
					}
					else
					{
						int kisanOutputIndex = outputIndex - DIGITAL_OUTPUT_COUNT;
						
						global._kisanOutputs[kisanOutputIndex] = !global._kisanOutputs[kisanOutputIndex];
					}

                    sprintf(response, "%s", command);
                }
            }
            else
            {
                sprintf(response, "%s %s", command, ERROR_INVALID_CONTROL_MODE);
            }
        }
		else if(strcmp(command, "SSAO"))
		{
			bool isPassedArgumentCheck = true;
			if(argumentIndex != 2)
			{
				sprintf(response, "%s %s", command, ERROR_INVALID_ARGUMENT_COUNT);
				isPassedArgumentCheck = false;
			}

			for(int i = 0; i < argumentIndex; i++)
			{
				for(int j = 0; j < argumentDataIndexs[i]; j++)
				{
					if(arguments[i][j] < '0' || arguments[i][j] > '9')
					{
						sprintf(response, "%s %s", command, ERROR_INVALID_ARGUMENT_FORMAT);
						isPassedArgumentCheck = false;
						break;
					}
				}
			}

			int outputIndex = 0;
			int outputTargetValue = 0;

			if(isPassedArgumentCheck)
			{
				outputIndex = atoi(arguments[0]);
				outputTargetValue = atoi(arguments[1]);

				if (outputIndex < 0 || outputIndex >= (KISAN_ANALOG_OUTPUT_COUNT) ||
				outputTargetValue < 0 || outputTargetValue > 20000)
				{
					sprintf(response, "%s %s", command, ERROR_INVALID_ARGUMENT_RANGE);
					isPassedArgumentCheck = false;
				}
			}

			if(isPassedArgumentCheck)
			{
				global._analogOutputsKisanTargets[outputIndex] = outputTargetValue;
				sprintf(response, "%s", command);
			}
		}
        else
        {
            sprintf(response, "%s %s", command, ERROR_NOT_SUPPORTED_COMMAND);
        }
    }

    _serial->write(0x02);
    _serial->print(response);
    _serial->write(0x03);
    _serial->flush();

    ClearReceiveBuffer();
}

void RemoteServer::Receive()
{
    while(_serial->available() > 0)
    {
        int readData = _serial->read();

        if(readData == -1)
        {
            break;
        }

        _buffer[_bufferIndex] = (char)readData;
        _bufferIndex++;
    }
}

int RemoteServer::FindCharInBuffer(char ch)
{
    int index = -1;
    for(int i=0; i<_bufferIndex; i++)
    {
        if (_buffer[i] == ch)
        {
            index = i;
            break;
        }
    }
    return index;
}

void RemoteServer::ClearReceiveBuffer()
{
    memset(_buffer, 0, sizeof(char) * RECEIVE_BUFFER_SIZE);
    _bufferIndex = 0;
}
