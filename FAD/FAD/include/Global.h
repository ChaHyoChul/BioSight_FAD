#ifndef GLOBAL_H_
#define GLOBAL_H_

#include "SystemMode.h"
#include "ControlMode.h"

class Global
{
public:
    SystemMode _systemMode;
    ControlMode _controlMode;

    bool *_digitalInputs;
    bool *_digitalInputsClicked;
    bool *_digitalOutputs;
    float *_analogInputs;
    double *_analogInputsKisan;
	bool *_kisanOutputs;

public:
    Global();
    ~Global();
};

extern Global global;

#endif
