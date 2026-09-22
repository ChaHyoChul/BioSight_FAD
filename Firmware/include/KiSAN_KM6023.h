#ifndef KISAN_KM6023_H_
#define KISAN_KM6023_H_

#include "KiSANModule.h"

class KiSAN_KM6023 : public KiSANModule
{
public:
    explicit KiSAN_KM6023(SerialPort &serial);

protected:
    void Send() override;
    void ProcessReceive() override;
};

#endif
