#ifndef KISAN_KM6063_H_
#define KISAN_KM6063_H_

#include "KiSANModule.h"

class KiSAN_KM6063 : public KiSANModule
{
public:
    explicit KiSAN_KM6063(SerialPort &serial);

protected:
    void Send() override;
    void ProcessReceive() override;
};

#endif
