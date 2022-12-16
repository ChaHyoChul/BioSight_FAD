#include "BuiltInLED.h"

#include <Arduino.h>

BuiltInLED::BuiltInLED()
{
}

BuiltInLED::~BuiltInLED()
{
}

void BuiltInLED::Initialize(unsigned int interval)
{
    _interval = interval;
    pinMode(LED_BUILTIN, OUTPUT);
}

void BuiltInLED::Blink()
{
    if (IsNeedToBlink() == false)
    {
        return;
    }

    if (IsTurnOn())
    {
        TurnOff();
    }
    else
    {
        TurnOn();
    }
}

bool BuiltInLED::IsNeedToBlink()
{
    return _timer.IsTimeout();
}

bool BuiltInLED::IsTurnOn()
{
    uint8_t bit = digitalPinToBitMask(LED_BUILTIN);
    uint8_t port = digitalPinToPort(LED_BUILTIN);
    if (port == NOT_A_PIN)
    {
        return false;
    }

    return (*portOutputRegister(port) & bit) ? true : false;
}

void BuiltInLED::TurnOn()
{
    digitalWrite(LED_BUILTIN, HIGH);

    _timer.Start(_interval);
}

void BuiltInLED::TurnOff()
{
    digitalWrite(LED_BUILTIN, LOW);

    _timer.Start(_interval);
}
