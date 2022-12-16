#include <Arduino.h>

#include "BuiltInLED.h"

BuiltInLED builtInLed;

void setup()
{
    builtInLed.Initialize(1000);
}

void loop()
{
    builtInLed.Blink();
}
