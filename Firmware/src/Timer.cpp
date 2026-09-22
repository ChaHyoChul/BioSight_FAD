#include "Timer.h"
#include "Board.h"

Timer::Timer()
: _timeoutMillis(0), _startTime(0), _isStarted(false)
{
}

void Timer::Start(uint32_t timeoutMillis)
{
    _timeoutMillis = timeoutMillis;
    _startTime = Board::Millis();
    _isStarted = true;
}

bool Timer::IsTimeout() const
{
    return (uint32_t)(Board::Millis() - _startTime) >= _timeoutMillis;
}

bool Timer::IsStarted() const
{
    return _isStarted;
}

void Timer::Reset()
{
    _timeoutMillis = 0;
    _startTime = Board::Millis();
    _isStarted = false;
}
