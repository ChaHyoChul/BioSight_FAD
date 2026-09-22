#include "PairedIOControlManager.h"
#include "Board.h"
#include "Global.h"
#include "GlobalDefinition.h"
#include "IOPair.h"

static const IOPair IO_PAIRS[] =
{
    { 2, 0 },
    { 3, 1 },
    { 4, 2 },
    { 5, 3 },
    { 6, 4 },
    { 7, 8 },
    { 8, 9 },
    { 9, 10 },
    { 10, 11 }
};

#define IO_PAIR_COUNT (sizeof(IO_PAIRS) / sizeof(IO_PAIRS[0]))

void ProcessPairedIOControl()
{
    if(global._controlMode != ControlMode::CM_Manual)
    {
        return;
    }

    for(uint8_t i = 0; i < IO_PAIR_COUNT; i++)
    {
        uint8_t outputIndex = IO_PAIRS[i].OutputIndex;
        bool state = global._digitalInputsClicked[IO_PAIRS[i].InputIndex];

        if(outputIndex < DIGITAL_OUTPUT_COUNT)
        {
            Board::WriteDigitalOutput(outputIndex, state);
            continue;
        }

        uint8_t kisanIndex = (uint8_t)(outputIndex - DIGITAL_OUTPUT_COUNT);
        if(kisanIndex < KISAN_DIGITAL_OUTPUT_COUNT)
        {
            global._kisanOutputs[kisanIndex] = state;
        }
    }
}
