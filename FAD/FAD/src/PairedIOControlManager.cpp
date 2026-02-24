#include "PairedIOControlManager.h"
#include "Global.h"
#include "Arduino.h"
#include "GlobalDefinition.h"

//#define PAIRED_IO_LIST_SIZE (5)
#define PAIRED_IO_LIST_SIZE (9)

PairedIOControlManager::PairedIOControlManager()
{
    ioPairs = new IOPair[PAIRED_IO_LIST_SIZE];

    SetIOPair(0, 2, 0);		// main valve 
    SetIOPair(1, 3, 1);		// group 1 
    SetIOPair(2, 4, 2);		// group 2 
    SetIOPair(3, 5, 3);		// group 3
	SetIOPair(4, 6, 4);		// group 4
	
    SetIOPair(5, 8, 8);		// 
    SetIOPair(6, 9, 9);
    SetIOPair(7, 10, 10);
    SetIOPair(8, 11, 11);
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
