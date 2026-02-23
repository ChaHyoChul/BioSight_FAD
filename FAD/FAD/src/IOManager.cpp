#include "IOManager.h"
#include "GlobalDefinition.h"
#include <Arduino.h>
#include "Global.h"

#define INPUT_CHATTERING_TIMEOUT_MILLISECOND (100)

IOManager::IOManager()
{
    _changedStateCountInputs = new int[DIGITAL_INPUT_COUNT];
    memset(_changedStateCountInputs, 0, sizeof(int) * DIGITAL_INPUT_COUNT);

    _inputChatteringTimer = new Timer[DIGITAL_INPUT_COUNT];
    for(int i=0; i<DIGITAL_INPUT_COUNT; i++)
    {
        _inputChatteringTimer[i].Reset();
    }
}

IOManager::~IOManager()
{
    delete [] _changedStateCountInputs;
    delete [] _inputChatteringTimer;
}

void IOManager::Initialize()
{
	int fanOutput1 = 6;
	int fanOutput2 = 7;
	
    for(int inputPin = DIGITAL_INPUT_FIRST_PIN; inputPin <= DIGITAL_INPUT_LAST_PIN; inputPin++)
    {
        pinMode(inputPin, INPUT);
    }

    for(int outputPin = DIGITAL_OUTPUT_FIRST_PIN; outputPin <= DIGITAL_OUTPUT_LAST_PIN; outputPin++)
    {
        pinMode(outputPin, OUTPUT);
    }
	
	digitalWrite(DIGITAL_OUTPUT_FIRST_PIN + fanOutput1, LOW);
	digitalWrite(DIGITAL_OUTPUT_FIRST_PIN + fanOutput2, LOW);
}

void IOManager::Process()
{
    for(int digitalInputPin = DIGITAL_INPUT_FIRST_PIN; digitalInputPin <= DIGITAL_INPUT_LAST_PIN; digitalInputPin++)
    {
        int index = digitalInputPin - DIGITAL_INPUT_FIRST_PIN;

        int state = digitalRead(digitalInputPin);
        bool newState = newState = (state == HIGH);

        if(global._digitalInputs[index] != newState)
        {
            if(_inputChatteringTimer[index].IsStarted())
            {
                if(_inputChatteringTimer[index].IsTimeout())
                {
                    if(global._digitalInputs[index] != newState)
                    {
                        global._digitalInputs[index] = newState;

                        if(global._controlMode == ControlMode::CM_Manual)
                        {
                            _changedStateCountInputs[index] += 1;

                            if(_changedStateCountInputs[index] >= 2)
                            {
                                global._digitalInputsClicked[index] = !global._digitalInputsClicked[index];
                                _changedStateCountInputs[index] = 0;
                            }
                        }
                    }
                    else
                    {
                        _changedStateCountInputs[index] = 0;
                    }

                    _inputChatteringTimer[index].Reset();
                }
            }
            else
            {
                _inputChatteringTimer[index].Start(INPUT_CHATTERING_TIMEOUT_MILLISECOND);
            }
        }
        else
        {
            if(_inputChatteringTimer[index].IsStarted())
            {
                if(_inputChatteringTimer[index].IsTimeout())
                {
                    _changedStateCountInputs[index] = 0;

                    _inputChatteringTimer[index].Reset();
                }
            }
        }
    }

    for(int digitalOutputPin = DIGITAL_OUTPUT_FIRST_PIN; digitalOutputPin <= DIGITAL_OUTPUT_LAST_PIN; digitalOutputPin++)
    {
        uint8_t bit = digitalPinToBitMask((uint8_t)digitalOutputPin);
        uint8_t port = digitalPinToPort((uint8_t)digitalOutputPin);
        if(port == NOT_A_PIN)
        {
            continue;
        }

        int index = digitalOutputPin - DIGITAL_OUTPUT_FIRST_PIN;
        global._digitalOutputs[index] = (*portOutputRegister(port) & bit) ? true : false;
    }

    for(int analogInputPin = A0; analogInputPin <= A3; analogInputPin++)
    {
        long data = analogRead(analogInputPin);
        
        int index = analogInputPin - A0;
        global._analogInputs[index] = data * 20.0 / 1023.0;
    }
}

// Lock Type 버튼 사용할 경우 
/*
void IOManager::Process()
{
	for(int digitalInputPin = DIGITAL_INPUT_FIRST_PIN; digitalInputPin <= DIGITAL_INPUT_LAST_PIN; digitalInputPin++)
	{
		int index = digitalInputPin - DIGITAL_INPUT_FIRST_PIN;

		int state = digitalRead(digitalInputPin);
		bool newState = newState = (state == HIGH);

		if(global._digitalInputs[index] != newState)
		{
			if(_inputChatteringTimer[index].IsStarted())
			{
				if(_inputChatteringTimer[index].IsTimeout())
				{
					if(global._digitalInputs[index] != newState)
					{
						global._digitalInputs[index] = newState;

						if(global._controlMode == ControlMode::CM_Manual)
						{
// 							_changedStateCountInputs[index] += 1;
// 
// 							if(_changedStateCountInputs[index] >= 2)
// 							{
// 								global._digitalInputsClicked[index] = !global._digitalInputsClicked[index];
// 								_changedStateCountInputs[index] = 0;
// 							}
							
							global._digitalInputsClicked[index] = global._digitalInputs[index];
							_changedStateCountInputs[index] = 0;
						}
					}
					else
					{
						_changedStateCountInputs[index] = 0;
					}

					_inputChatteringTimer[index].Reset();
				}
			}
			else
			{
				_inputChatteringTimer[index].Start(INPUT_CHATTERING_TIMEOUT_MILLISECOND);
			}
		}
		else
		{
			if(_inputChatteringTimer[index].IsStarted())
			{
				if(_inputChatteringTimer[index].IsTimeout())
				{
					_changedStateCountInputs[index] = 0;

					_inputChatteringTimer[index].Reset();
				}
			}
		}
	}

	for(int digitalOutputPin = DIGITAL_OUTPUT_FIRST_PIN; digitalOutputPin <= DIGITAL_OUTPUT_LAST_PIN; digitalOutputPin++)
	{
		uint8_t bit = digitalPinToBitMask((uint8_t)digitalOutputPin);
		uint8_t port = digitalPinToPort((uint8_t)digitalOutputPin);
		if(port == NOT_A_PIN)
		{
			continue;
		}

		int index = digitalOutputPin - DIGITAL_OUTPUT_FIRST_PIN;
		global._digitalOutputs[index] = (*portOutputRegister(port) & bit) ? true : false;
	}

	for(int analogInputPin = A0; analogInputPin <= A3; analogInputPin++)
	{
		long data = analogRead(analogInputPin);
		
		int index = analogInputPin - A0;
		global._analogInputs[index] = data * 20.0 / 1023.0;
	}
}
*/
