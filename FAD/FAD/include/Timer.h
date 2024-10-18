#ifndef TIMER_H_
#define TIMER_H_

class Timer
{
private:
    unsigned long _timeoutMillis;
    unsigned long _startTime;
    bool _isStarted;

public:
    Timer();
    ~Timer();

public:
    void Start(unsigned long timeoutMillis);
    bool IsTimeout();
    bool IsStarted();
    void Reset();
};

#endif
