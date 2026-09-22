#ifndef BUILTINLED_H_
#define BUILTINLED_H_

#include <stdint.h>
#include "Timer.h"

class BuiltInLED
{
private:
    uint32_t _interval = 0;
    Timer _timer;

public:
    void Initialize(uint32_t interval);
    void Blink();
};

#endif
