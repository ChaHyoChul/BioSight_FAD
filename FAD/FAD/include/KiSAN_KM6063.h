#ifndef KISAN_KM6063_H_
#define KISAN_KM6063_H_

#include <Arduino.h>
#include "KiSANModule.h"
#include "Timer.h"

class KiSAN_KM6063 : public KiSANModule
{
private:
	HardwareSerial *_hardwareSerial;
	int _state;
	bool _isReceivedStx;
	bool _isReceivedEtx;
	char *_receiveBuffer;
	int _receiveBufferIndex;

	int _retryCount;
	bool _isConnected;

	Timer _receiveTimer;
	Timer _waitTimer;

public:
	KiSAN_KM6063();
	KiSAN_KM6063(unsigned int address, KiSANProtocol* kisanProtocol);
	~KiSAN_KM6063();

public:
	void Initialize(HardwareSerial *hardwareSerial);
	virtual bool Process();

private:
	void Send();
	int Receive();
	void ProcessReceive();
};

#endif
