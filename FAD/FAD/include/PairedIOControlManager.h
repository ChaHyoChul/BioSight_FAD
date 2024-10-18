#ifndef PAIREDIOCONTROLMANAGER_H_
#define PAIREDIOCONTROLMANAGER_H_

#include "IOPair.h"

class PairedIOControlManager
{
private:
    IOPair *ioPairs;

public:
    PairedIOControlManager();
    ~PairedIOControlManager();

public:
    void Process();

private:
    void SetIOPair(int index, int inputIndex, int outputIndex);
};

#endif
