#include "ExternalFanControl.h"
#include "Board.h"
#include "Global.h"
#include "GlobalDefinition.h"
#include "Timer.h"

#define CURRENT_LOOP_MIN_MA (4.0f)
#define CURRENT_LOOP_MAX_MA (20.0f)
#define OXYGEN_REFERENCE_AT_MIN (4.0f)
#define OXYGEN_REFERENCE_AT_MAX (25.0f)
#define OXYGEN_FAN_ON_THRESHOLD (19.0f)
#define OXYGEN_FAN_OFF_THRESHOLD (20.0f)
#define FAN_UPDATE_INTERVAL_MILLISECONDS (1000)

static void SetFans(bool state)
{
    Board::WriteDigitalOutput(FAN_FIRST_OUTPUT_INDEX, state);
    Board::WriteDigitalOutput(FAN_SECOND_OUTPUT_INDEX, state);
}

void ExternalFanControl()
{
    static Timer waitTimer;

    if(!waitTimer.IsStarted())
    {
        waitTimer.Start(FAN_UPDATE_INTERVAL_MILLISECONDS);
        return;
    }

    if(!waitTimer.IsTimeout())
    {
        return;
    }

    float current = global._analogInputsKisan[OXYGEN_SENSOR_INPUT_INDEX];
    float oxygenConcentration = (current - CURRENT_LOOP_MIN_MA) * (OXYGEN_REFERENCE_AT_MAX - OXYGEN_REFERENCE_AT_MIN)
        / (CURRENT_LOOP_MAX_MA - CURRENT_LOOP_MIN_MA) + OXYGEN_REFERENCE_AT_MIN;

    global._oxygenConcentration = oxygenConcentration;

    bool isSupplying = global._digitalOutputs[MAIN_VALVE_OUTPUT_INDEX];

    if(!isSupplying && oxygenConcentration > OXYGEN_FAN_OFF_THRESHOLD)
    {
        SetFans(false);
    }
    else if(isSupplying || oxygenConcentration < OXYGEN_FAN_ON_THRESHOLD)
    {
        SetFans(true);
    }

    waitTimer.Start(FAN_UPDATE_INTERVAL_MILLISECONDS);
}
