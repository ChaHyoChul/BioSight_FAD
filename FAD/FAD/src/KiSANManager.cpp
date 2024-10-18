#include "KiSANManager.h"

KiSANManager::KiSANManager()
{
	_km6015 = NULL;
	_km6063 = NULL;
	_currentModule = 0;
	
	_waitTimer.Reset();
}

KiSANManager::~KiSANManager()
{
	if(_km6015 != NULL)
	{
		delete _km6015;
	}
	
	if(_km6063 != NULL)
	{
		delete _km6063;
	}
}

void KiSANManager::Initialize(HardwareSerial* hardwareSerial, int baudRate)
{
	_hardwareSerial = hardwareSerial;
	_hardwareSerial->begin(baudRate);
	
	_km6015 = new KiSAN_KM6015(1, NULL);
	_km6015->Initialize(_hardwareSerial);
	
	_km6063 = new KiSAN_KM6063(2, NULL);
	_km6063->Initialize(_hardwareSerial);
}

void KiSANManager::Process()
{
	bool result = false;
	switch(_currentModule)
	{
		case 0:
		if(_km6063->Process())
		{
			_currentModule = 1;
		}
		break;
		
		case 1:
		if(_km6015->Process())
		{
			_currentModule = 0;
		}
		break;
		
		default:
		_currentModule = 0;
	}
}
