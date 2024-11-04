#ifndef KISAN_KM6023_H_
#define KISAN_KM6023_H_

#include "KiSANModule.h"
#include "Timer.h"
#include <Arduino.h>

class KiSAN_KM6023 : public KiSANModule
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
	KiSAN_KM6023();
	KiSAN_KM6023(unsigned int address, KiSANProtocol* kisanProtocol);
	~KiSAN_KM6023();

	public:
	void Initialize(HardwareSerial *hardwareSerial);
	virtual bool Process();

	private:
	void Send();
	int Receive();
	void ProcessReceive();
};

#endif
