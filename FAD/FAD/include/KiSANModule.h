#ifndef KISANMODULE_H_
#define KISANMODULE_H_

#include "KiSANProtocol.h"

class KiSANModule
{
public:
    unsigned int _address;
    KiSANProtocol *_kisanProtocol;

public:
    KiSANModule(unsigned int address, KiSANProtocol* kisanProtocol);
    ~KiSANModule();

    virtual bool Process() = 0;
};

#endif
