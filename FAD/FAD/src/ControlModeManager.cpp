#include "ControlModeManager.h"
#include "Global.h"
#include "GlobalDefinition.h"
#include <Arduino.h>

ControlModeManager::ControlModeManager()
{
}

ControlModeManager::~ControlModeManager()
{
}

void ControlModeManager::Process()
{
	// TODO: 인천 환경과학원 컨트롤 모드 변경 스위치에 대한 조건처리.
	// TODO: 다른 애들은 다르게 판정해야 함.
	// TODO - hccha: 주석문만 제거 
	//	DIGITAL_OUTPUT_FIRST_PIN은 왜 다시 ON 하는지? 확인 필요 
	if(global._digitalInputs[15] == false)
	{
		if(global._controlMode != ControlMode::CM_Emergency)
		{
			global._controlMode = ControlMode::CM_Emergency;

			for(int i=0; i<DIGITAL_INPUT_COUNT; i++)
			{
				global._digitalInputsClicked[i] = false;
			}

			digitalWrite(DIGITAL_OUTPUT_FIRST_PIN, LOW);
			for(int i=DIGITAL_OUTPUT_FIRST_PIN + 1; i <= DIGITAL_OUTPUT_LAST_PIN; i++)
			{
				digitalWrite(i, LOW);
			}
			digitalWrite(DIGITAL_OUTPUT_FIRST_PIN + 5, HIGH);
		}
		
		return;
	}
	
	if(global._digitalInputs[0] == true || global._digitalInputs[1] == false)
	{
		if(global._controlMode != ControlMode::CM_Manual)
		{
			global._controlMode = ControlMode::CM_Manual;

			for(int i=0; i<DIGITAL_INPUT_COUNT; i++)
			{
				global._digitalInputsClicked[i] = false;
			}
		}
	}
	else if(global._digitalInputs[0] == false || global._digitalInputs[1] == true)
	{
		if(global._controlMode != ControlMode::CM_Auto)
		{
			global._controlMode = ControlMode::CM_Auto;

			for(int i=DIGITAL_OUTPUT_FIRST_PIN; i<=DIGITAL_OUTPUT_LAST_PIN; i++)
			{
				digitalWrite(i, LOW);
			}
		}
	}
	else
	{
		if(global._controlMode != ControlMode::CM_Undefined)
		{
			global._controlMode = ControlMode::CM_Undefined;
		}
	}
	
	//if(global._digitalInputs[0] == true)
	//{
	//if(global._controlMode != ControlMode::CM_Auto)
	//{
	//global._controlMode = ControlMode::CM_Auto;
	//
	//for(int i=DIGITAL_OUTPUT_FIRST_PIN; i<=DIGITAL_OUTPUT_LAST_PIN; i++)
	//{
	//digitalWrite(i, LOW);
	//}
	//}
	//}
	//else
	//{
	//if(global._controlMode != ControlMode::CM_Manual)
	//{
	//global._controlMode = ControlMode::CM_Manual;
	//
	//for(int i=0; i<DIGITAL_INPUT_COUNT; i++)
	//{
	//global._digitalInputsClicked[i] = false;
	//}
	//}
	//}
}
