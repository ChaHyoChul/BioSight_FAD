#include "Timer.h"
#include <Arduino.h>

Timer::Timer()
{
    Reset();
}

Timer::~Timer()
{
}

void Timer::Start(unsigned long timeoutMillis)
{
    _timeoutMillis = timeoutMillis;
    _startTime = millis();
    _isStarted = true;
}

bool Timer::IsTimeout()
{
    unsigned long currentTime = millis();

    if(currentTime < _startTime)
    {
        _startTime = millis();
        currentTime = millis();
        return false;
    }

    bool isTimeout = ((currentTime - _startTime) > _timeoutMillis);
    return isTimeout;
}

bool Timer::IsStarted()
{
    return _isStarted;
}

void Timer::Reset()
{
    _timeoutMillis = 0;
    _startTime = millis();
    _isStarted = false;
}
