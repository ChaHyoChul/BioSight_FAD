#ifndef TIMER_H_
#define TIMER_H_

#include <stdint.h>

class Timer
{
private:
    uint32_t _timeoutMillis;
    uint32_t _startTime;
    bool _isStarted;

public:
    Timer();

public:
    void Start(uint32_t timeoutMillis);
    bool IsTimeout() const;
    bool IsStarted() const;
    void Reset();
};

#endif
