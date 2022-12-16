#include "Timer.h"

#include <Arduino.h>

Timer::Timer()
{
    _timeoutMillis = 0;
    _startTime = millis();
}

Timer::~Timer()
{
}

void Timer::Start(unsigned long timeoutMillis)
{
    _timeoutMillis = timeoutMillis;
    _startTime = millis();
}

bool Timer::IsTimeout()
{
    unsigned long currentTime = millis();

    if(currentTime < _startTime)
    {
        currentTime = millis();
        _startTime = millis();
        return false;
    }

    return currentTime - _startTime > _timeoutMillis;
}
