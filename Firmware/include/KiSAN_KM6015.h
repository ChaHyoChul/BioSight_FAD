#ifndef KISAN_KM6015_H_
#define KISAN_KM6015_H_

#include "KiSANModule.h"

class KiSAN_KM6015 : public KiSANModule
{
public:
    explicit KiSAN_KM6015(SerialPort &serial);

protected:
    void Send() override;
    void ProcessReceive() override;
};

#endif
