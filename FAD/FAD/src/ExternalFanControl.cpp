/*
 * ExternalFanControl.cpp
 *
 * Created: 2025-01-17 오후 1:44:26
 *  Author: bychu
 */ 

#include "ExternalFanControl.h"
#include "Global.h"
#include "GlobalDefinition.h"
#include "Timer.h"
#include <Arduino.h>

// 인천 환경과학원 - 생물 자원관 
// 공급중이거나 산소 농도가 19이하 이면, FAN ON 
// 공급중이 아니고 산소농도가 20이상이면, FAN OFF 
/*
analog 데이터를 사용해서 산소농도 계산 
public double OxygenConcentrationCalibration(double current)
{
  return (current - _lowValue) * (OxygenConcentration20mARef - OxygenConcentration4mARef) / (_highValue - _lowValue) + OxygenConcentration4mARef;
}
*/

// void ExternalFanControl()
// {
// 	static Timer tmWait;
// 	
// 	int mainValveOutput = 0;				// 메일 벨브 번호로 변경 
// 	int fanOutput1 = 6;						// FAN 출력 번호로 변경 
// 	int fanOutput2 = 7;						// FAN 출력 번호로 변경 
// 	float oxygenConcentration = 0.0;
// // 	float f4mARef = 4;						// 4mARef 값으로 변경 (BBMS 프로그램 데이터 참조) 
// // 	float f20mARef = 20;					// 20mARef 값으로 변경 (BBMS 프로그램 데이터 참조) 
//  	float f4mARef = 4;						// 4mARef 값으로 변경 (BBMS 프로그램 데이터 참조)
//  	float f20mARef = 25;					// 20mARef 값으로 변경 (BBMS 프로그램 데이터 참조)
// 
// 	if (tmWait.IsStarted())
// 	{
// 		if (tmWait.IsTimeout())
// 		{
// 			oxygenConcentration = (float)((global._analogInputsKisan[2] - 4.0) * (f20mARef - f4mARef) / (20.0 - 4.0) + f4mARef);
// 			global._oxygenConcentration = oxygenConcentration;
// 
// 			if (global._digitalOutputs[mainValveOutput] == 0 &&
// 				oxygenConcentration > 20.0)
// // 				if (global._digitalOutputs[mainValveOutput] == 0 &&
// // 					global._analogInputsKisan[2] > 17.0)
// 			{
// 				digitalWrite(DIGITAL_OUTPUT_FIRST_PIN + fanOutput1, LOW);
// 				digitalWrite(DIGITAL_OUTPUT_FIRST_PIN + fanOutput2, LOW);
// // 				digitalWrite(DIGITAL_OUTPUT_FIRST_PIN + fanOutput1, HIGH);
// // 				digitalWrite(DIGITAL_OUTPUT_FIRST_PIN + fanOutput2, HIGH);
// 			}
// 			else 
// 			{
// 				digitalWrite(DIGITAL_OUTPUT_FIRST_PIN + fanOutput1, HIGH);
// 				digitalWrite(DIGITAL_OUTPUT_FIRST_PIN + fanOutput2, HIGH);
// // 				digitalWrite(DIGITAL_OUTPUT_FIRST_PIN + fanOutput1, LOW);
// // 				digitalWrite(DIGITAL_OUTPUT_FIRST_PIN + fanOutput2, LOW);
// 			}
// 		
// 			tmWait.Start(1000);
// 		}
// 	}
// 	else 
// 	{
// 		tmWait.Start(1000);
// 	}
// }

void ExternalFanControl()
{
	static Timer tmWait;
	
	int mainValveOutput = 0;				// 메일 벨브 번호로 변경
	int fanOutput1 = 6;						// FAN 출력 번호로 변경
	int fanOutput2 = 7;						// FAN 출력 번호로 변경
	float oxygenConcentration = 0.0;
	float f4mARef = 4;						// 4mARef 값으로 변경 (BBMS 프로그램 데이터 참조)
	float f20mARef = 25;					// 20mARef 값으로 변경 (BBMS 프로그램 데이터 참조)

	if (tmWait.IsStarted())
	{
		if (tmWait.IsTimeout())
		{
			oxygenConcentration = (float)((global._analogInputsKisan[2] - 4.0) * (f20mARef - f4mARef) / (20.0 - 4.0) + f4mARef);
			global._oxygenConcentration = oxygenConcentration;

// 			if (global._digitalOutputs[mainValveOutput] == 0 &&
// 				oxygenConcentration > 20.0)
// 			{
// 				// FAN OFF
// 				digitalWrite(DIGITAL_OUTPUT_FIRST_PIN + fanOutput1, LOW);
// 				digitalWrite(DIGITAL_OUTPUT_FIRST_PIN + fanOutput2, LOW);
// 			}
// 			else
// 			{
// 				// FAN ON
// 				digitalWrite(DIGITAL_OUTPUT_FIRST_PIN + fanOutput1, HIGH);
// 				digitalWrite(DIGITAL_OUTPUT_FIRST_PIN + fanOutput2, HIGH);
// 			}

			if (global._digitalOutputs[mainValveOutput] == 0 &&
				oxygenConcentration > 20.0)
			{
				// FAN OFF
				digitalWrite(DIGITAL_OUTPUT_FIRST_PIN + fanOutput1, LOW);
				digitalWrite(DIGITAL_OUTPUT_FIRST_PIN + fanOutput2, LOW);
			}
			else if (global._digitalOutputs[mainValveOutput] != 0 ||
				oxygenConcentration < 19.0)
			{
				// FAN ON
				digitalWrite(DIGITAL_OUTPUT_FIRST_PIN + fanOutput1, HIGH);
				digitalWrite(DIGITAL_OUTPUT_FIRST_PIN + fanOutput2, HIGH);					
			}
			
			tmWait.Start(1000);
		}
	}
	else
	{
		tmWait.Start(1000);
	}
}
