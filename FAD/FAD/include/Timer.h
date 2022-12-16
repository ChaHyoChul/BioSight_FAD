#ifndef TIMER_H_
#define TIMER_H_

class Timer
{
private:
    unsigned long _timeoutMillis;
    unsigned long _startTime;

public:
    Timer();
    ~Timer();

public:
    void Start(unsigned long timeoutMillis);
    bool IsTimeout();
};

#endif
