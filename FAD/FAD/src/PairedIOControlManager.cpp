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
	
    SetIOPair(5, 7, 8);		// aux1 (KM6065) 
    SetIOPair(6, 8, 9);		// aux2 (KM6065) 
    SetIOPair(7, 9, 10);	// aux3 (KM6065) 
    SetIOPair(8, 10, 11);	// aux4 (KM6065) 

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
		int outputIndex = ioPairs[i].OutputIndex;
        int state = global._digitalInputsClicked[inputIndex] ? HIGH : LOW;

		if (outputIndex < DIGITAL_OUTPUT_COUNT)
		{
			digitalWrite(DIGITAL_OUTPUT_FIRST_PIN + outputIndex, state);
		}
		else
		{
			int kisanIndex = outputIndex - DIGITAL_OUTPUT_COUNT;
			if (kisanIndex >= 0 && kisanIndex < KISAN_DIGITAL_OUTPUT_COUNT)
			{
				global._kisanOutputs[kisanIndex] = (state == HIGH);
			}
		}
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
