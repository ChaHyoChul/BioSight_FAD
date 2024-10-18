#include "KiSANModule.h"

KiSANModule::KiSANModule(unsigned int address, KiSANProtocol* kisanProtocol)
{
    _address = address;
    _kisanProtocol = kisanProtocol;
}

KiSANModule::~KiSANModule()
{
}
