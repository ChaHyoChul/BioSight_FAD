#include <Arduino.h>
#include "BuiltInLED.h"
#include "IOManager.h"
#include "RemoteServer.h"
#include "ControlModeManager.h"
#include "PairedIOControlManager.h"

#include "KiSAN_KM6015.h"
#include "KiSAN_KM6063.h"
#include "KiSAN_KM6023.h"

#include "ExternalFanControl.h"

BuiltInLED builtInLed;
IOManager ioManager;
RemoteServer remoteServer;
RemoteServer remoteServerDebug;
ControlModeManager controlModeManager;
PairedIOControlManager pairedIOControlManager;

KiSAN_KM6015 _km6015;
//KiSAN_KM6063 _km6063;
//KiSAN_KM6023 _km6023;
int _currentModule = 0;

void setup()
{
    builtInLed.Initialize(1000);
    ioManager.Initialize();
    remoteServer.Initialize(&Serial1, 9600);
	//Serial.begin(9600);
	remoteServerDebug.Initialize(&Serial, 9600);
	
	Serial3.begin(9600);
	_km6015.Initialize(&Serial3);
	//_km6063.Initialize(&Serial2);
	//_km6023.Initialize(&Serial2);
}

void loop()
{
    builtInLed.Blink();
    ioManager.Process();
    controlModeManager.Process();
    pairedIOControlManager.Process();
    remoteServer.Process();
	remoteServerDebug.Process();

	_km6015.Process();
	//_km6023.Process();

	ExternalFanControl();
	
	//switch(_currentModule)
	//{
		//case 0:
		//if(_km6063.Process())
		//{
			//_currentModule = 1;
		//}
		//break;
		//
		//case 1:
		//if(_km6015.Process())
		//{
			//_currentModule = 0;
		//}
		//break;
		//
		//default:
		//_currentModule = 0;
	//}
}
