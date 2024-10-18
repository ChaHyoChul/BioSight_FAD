#ifndef IOMANAGER_H_
#define IOMANAGER_H_

#include "Timer.h"

class IOManager
{
private:
    int *_changedStateCountInputs;
    Timer *_inputChatteringTimer;

public:
    IOManager();
    ~IOManager();

public:
    void Initialize();
    void Process();
};

#endif
