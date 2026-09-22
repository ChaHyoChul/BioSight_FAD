#include "ControlModeManager.h"
#include "Board.h"
#include "Global.h"
#include "GlobalDefinition.h"

#define MODE_SELECT_MANUAL_INPUT_INDEX (0)
#define MODE_SELECT_AUTO_INPUT_INDEX (1)

static void ClearClickedInputs()
{
    for(uint8_t i = 0; i < DIGITAL_INPUT_COUNT; i++)
    {
        global._digitalInputsClicked[i] = false;
    }
}

void ProcessControlMode()
{
    if(global._digitalInputs[EMERGENCY_INPUT_INDEX])
    {
        if(global._controlMode != ControlMode::CM_Emergency)
        {
            global._controlMode = ControlMode::CM_Emergency;

            ClearClickedInputs();
            Board::WriteDigitalOutputs((uint8_t)(1 << ALARM_OUTPUT_INDEX));
        }

        return;
    }

    if(global._controlMode == ControlMode::CM_Emergency)
    {
        Board::WriteDigitalOutput(ALARM_OUTPUT_INDEX, false);
    }

    if(global._digitalInputs[MODE_SELECT_MANUAL_INPUT_INDEX] || !global._digitalInputs[MODE_SELECT_AUTO_INPUT_INDEX])
    {
        if(global._controlMode != ControlMode::CM_Manual)
        {
            global._controlMode = ControlMode::CM_Manual;
            ClearClickedInputs();
        }

        return;
    }

    if(global._controlMode != ControlMode::CM_Auto)
    {
        global._controlMode = ControlMode::CM_Auto;
        Board::WriteDigitalOutputs(0);
    }
}
