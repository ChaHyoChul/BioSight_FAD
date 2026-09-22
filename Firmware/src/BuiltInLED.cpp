#include "BuiltInLED.h"
#include "Board.h"

void BuiltInLED::Initialize(uint32_t interval)
{
    _interval = interval;
    _timer.Start(_interval);
}

void BuiltInLED::Blink()
{
    if(!_timer.IsTimeout())
    {
        return;
    }

    Board::ToggleStatusLed();
    _timer.Start(_interval);
}
