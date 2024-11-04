#include "PairedIOControlManager.h"
#include "Global.h"
#include "Arduino.h"
#include "GlobalDefinition.h"

#define PAIRED_IO_LIST_SIZE (4)

PairedIOControlManager::PairedIOControlManager()
{
    ioPairs = new IOPair[PAIRED_IO_LIST_SIZE];

    SetIOPair(0, 2, 0);
    SetIOPair(1, 3, 1);
    SetIOPair(2, 4, 2);
    SetIOPair(3, 5, 3);
    //SetIOPair(4, 6, 4);
    //SetIOPair(5, 7, 5);
    //SetIOPair(6, 8, 6);
    //SetIOPair(7, 9, 7);
}

PairedIOControlManager::~PairedIOControlManager()
{
    delete [] ioPairs;
}

void PairedIOControlManager::Process()
{
    if(global._controlMode != ControlMode::CM_Manual)
    {
        return;
    }

    for(int i=0; i<PAIRED_IO_LIST_SIZE; i++)
    {
        int inputIndex = ioPairs[i].InputIndex;
        int state = global._digitalInputsClicked[inputIndex] ? HIGH : LOW;
        digitalWrite(DIGITAL_OUTPUT_FIRST_PIN + ioPairs[i].OutputIndex, state);
		//if(ioPairs[i].OutputIndex == 0)
		//{
			//digitalWrite(DIGITAL_OUTPUT_FIRST_PIN + ioPairs[i].OutputIndex + 5, state);
		//}
    }
}

void PairedIOControlManager::SetIOPair(int index, int inputIndex, int outputIndex)
{
    if(index >= PAIRED_IO_LIST_SIZE)
    {
        return;
    }

    ioPairs[index].InputIndex = inputIndex;
    ioPairs[index].OutputIndex = outputIndex;
}
