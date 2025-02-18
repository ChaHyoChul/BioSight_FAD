#include "Global.h"
#include <Arduino.h>
#include "GlobalDefinition.h"

Global::Global()
{
    _systemMode = SystemMode::SM_Release;
    _controlMode = ControlMode::CM_Undefined;

    _digitalInputs = new bool[DIGITAL_INPUT_COUNT];
    memset(_digitalInputs, 0, sizeof(bool) * DIGITAL_INPUT_COUNT);

    _digitalOutputs = new bool[DIGITAL_OUTPUT_COUNT];
    memset(_digitalOutputs, 0, sizeof(bool) * DIGITAL_OUTPUT_COUNT);

    _analogInputs = new float[ANALOG_INPUT_COUNT];
    for(int i=0; i<ANALOG_INPUT_COUNT; i++)
    {
        _analogInputs[i] = 0.0;
    }

    _digitalInputsClicked = new bool[DIGITAL_INPUT_COUNT];
    memset(_digitalInputsClicked, 0, sizeof(bool) * DIGITAL_INPUT_COUNT);

    _analogInputsKisan = new double[KISAN_ANALOG_INPUT_COUNT];
    for(int i=0; i<KISAN_ANALOG_INPUT_COUNT; i++)
    {
        _analogInputsKisan[i] = 0.0;
    }
	
	_kisanOutputs = new bool[KISAN_DIGITAL_OUTPUT_COUNT];
	memset(_kisanOutputs, 0, sizeof(bool) * KISAN_DIGITAL_OUTPUT_COUNT);
	
	
	_analogOutputsKisan = new double[KISAN_ANALOG_OUTPUT_COUNT];
	for(int i=0; i<KISAN_ANALOG_OUTPUT_COUNT; i++)
	{
		_analogOutputsKisan[i] = 0.0;
	}
	
	_analogOutputsKisanTargets = new double[KISAN_ANALOG_OUTPUT_COUNT];
	for(int i=0; i<KISAN_ANALOG_OUTPUT_COUNT; i++)
	{
		_analogOutputsKisanTargets[i] = 0.0;
	}
	
	_oxygenConcentration = 0.0;
}

Global::~Global()
{
    delete [] _digitalInputs;
    delete [] _digitalOutputs;
    delete [] _analogInputs;
    delete [] _digitalInputsClicked;
	delete [] _analogInputsKisan;
	delete [] _kisanOutputs;
	delete [] _analogOutputsKisan;
	delete [] _analogOutputsKisanTargets;
}

Global global;
