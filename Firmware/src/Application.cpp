#include "Application.h"
#include "Board.h"
#include "BuiltInLED.h"
#include "ControlModeManager.h"
#include "GlobalDefinition.h"
#include "IOManager.h"
#include "KiSAN_KM6015.h"
#include "KiSAN_KM6063.h"
#include "PairedIOControlManager.h"
#include "RemoteServer.h"
#include "SerialPort.h"

static BuiltInLED builtInLed;
static IOManager ioManager;
static RemoteServer remoteServer(HostSerial);
static RemoteServer remoteServerDebug(DebugSerial);

static KiSAN_KM6015 km6015(FieldbusSerial);
static KiSAN_KM6063 km6063(FieldbusSerial);
static KiSANModule *const kisanModules[] = { &km6015, &km6063 };
static uint8_t currentKisanModule = 0;

#define KISAN_MODULE_COUNT (sizeof(kisanModules) / sizeof(kisanModules[0]))

void Application::Initialize()
{
    Board::Initialize();

    HostSerial.Begin(SERIAL_BAUD_RATE);
    DebugSerial.Begin(SERIAL_BAUD_RATE);
    FieldbusSerial.Begin(SERIAL_BAUD_RATE);

    builtInLed.Initialize(BUILTIN_LED_INTERVAL_MILLISECONDS);
}

void Application::RunCycle()
{
    builtInLed.Blink();
    ioManager.Process();
    ProcessControlMode();
    ProcessPairedIOControl();

    remoteServer.Process();
    remoteServerDebug.Process();

    if(kisanModules[currentKisanModule]->Process())
    {
        currentKisanModule = (uint8_t)((currentKisanModule + 1) % KISAN_MODULE_COUNT);
    }
}
