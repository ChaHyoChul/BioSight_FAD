#ifndef BUILTINLED_H_
#define BUILTINLED_H_

#include "Timer.h"

class BuiltInLED
{
private:
    unsigned int _interval;
    Timer _timer;

public:
    BuiltInLED();
    ~BuiltInLED();

public:
    void Initialize(unsigned int interval);
    void Blink();

private:
    bool IsNeedToBlink();
    bool IsTurnOn();
    void TurnOn();
    void TurnOff();
};

#endif
