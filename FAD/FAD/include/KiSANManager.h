#ifndef KISANMANAGER_H_
#define KISANMANAGER_H_

#include <Arduino.h>
#include "KiSANModule.h"
#include "KiSAN_KM6015.h"
#include "KiSAN_KM6063.h"
#include "Timer.h"

class KiSANManager
{
private:
	HardwareSerial* _hardwareSerial;
	KiSAN_KM6015* _km6015;
	KiSAN_KM6063* _km6063;
	int _currentModule;
	Timer _waitTimer;
	
public:
	KiSANManager();
	~KiSANManager();
	
public:
	void Initialize(HardwareSerial* hardwareSerial, int baudRate);
	void Process();
};

#endif
