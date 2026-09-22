#include <stdio.h>
#include <string.h>
#include <chrono>
#include <cmath>
#include <limits>
#include <string>
#include <vector>
#include "Board.h"
#include "BuiltInLED.h"
#include "ChecksumCalculator.h"
#include "ControlModeManager.h"
#include "ExternalFanControl.h"
#include "Global.h"
#include "HostBoard.h"
#include "IOManager.h"
#include "KiSAN_KM6015.h"
#include "KiSAN_KM6023.h"
#include "KiSAN_KM6063.h"
#include "PairedIOControlManager.h"
#include "RemoteServer.h"
#include "SerialPort.h"
#include "TextFormat.h"
#include "Timer.h"

static int checks = 0;
static int failures = 0;

static void Check(bool condition, const char *text, const char *file, int line)
{
    checks++;
    if(!condition)
    {
        failures++;
        printf("FAIL %s:%d %s\n", file, line, text);
    }
}

static std::string Printable(const std::string &text)
{
    std::string result;
    for(char ch : text)
    {
        if(ch == 0x02)
        {
            result += "<STX>";
        }
        else if(ch == 0x03)
        {
            result += "<ETX>";
        }
        else if(ch == '\r')
        {
            result += "<CR>";
        }
        else
        {
            result.push_back(ch);
        }
    }
    return result;
}

static void CheckText(const std::string &actual, const std::string &expected, const char *file, int line)
{
    checks++;
    if(actual != expected)
    {
        failures++;
        printf("FAIL %s:%d\n  expected: %s\n  actual:   %s\n", file, line, Printable(expected).c_str(), Printable(actual).c_str());
    }
}

#define CHECK(condition) Check((condition), #condition, __FILE__, __LINE__)
#define CHECK_TEXT(actual, expected) CheckText((actual), (expected), __FILE__, __LINE__)

static const char STX = 0x02;
static const char ETX = 0x03;

static std::string Frame(const std::string &body)
{
    return std::string(1, STX) + body + std::string(1, ETX);
}

static void ResetAll()
{
    HostBoard::Reset();
    global = Global();
}

static std::string Exchange(RemoteServer &server, const std::string &request)
{
    HostBoard::Feed(HostSerial, Frame(request));
    server.Process();
    return HostBoard::Drain(HostSerial);
}

static std::vector<std::string> Fields(const std::string &response)
{
    std::string body = response.substr(1, response.size() - 2);
    size_t space = body.find(' ');
    std::vector<std::string> fields;
    if(space == std::string::npos)
    {
        return fields;
    }

    size_t start = space + 1;
    for(;;)
    {
        size_t comma = body.find(',', start);
        fields.push_back(body.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
        if(comma == std::string::npos)
        {
            return fields;
        }
        start = comma + 1;
    }
}

static void Advance(uint32_t milliseconds)
{
    HostBoard::Millis += milliseconds;
}

static void TestChecksumOfDocumentedFrames()
{
    CHECK(CalculateChecksum("#01G8208", 8) == 0x9D);
    CHECK(CalculateChecksum("#02S0001,00C0", 13) == 0x98);
    CHECK(CalculateChecksum("#02S0001,0000", 13) == 0x85);
    CHECK(CalculateChecksum("#01S0204,0FA0,0000,0000,0000", 28) == 0x74);
}

static void TestTimerSurvivesMillisWraparound()
{
    ResetAll();
    HostBoard::Millis = 0xFFFFFF00u;

    Timer timer;
    CHECK(!timer.IsStarted());
    timer.Start(1000);
    CHECK(timer.IsStarted());

    Advance(999);
    CHECK(!timer.IsTimeout());
    Advance(1);
    CHECK(timer.IsTimeout());

    timer.Reset();
    CHECK(!timer.IsStarted());
}

static void TestSerialPortBuffersAndLimits()
{
    ResetAll();
    CHECK(HostSerial.WriteSpace() == SERIAL_TX_BUFFER_SIZE - 1);

    for(int i = 0; i < SERIAL_TX_BUFFER_SIZE - 1; i++)
    {
        CHECK(HostSerial.Write((uint8_t)('A' + i % 26)));
    }
    CHECK(!HostSerial.Write((uint8_t)'Z'));
    CHECK(!HostSerial.Write("X", 1));
    CHECK(HostBoard::Drain(HostSerial).size() == SERIAL_TX_BUFFER_SIZE - 1);
    CHECK(HostSerial.WriteSpace() == SERIAL_TX_BUFFER_SIZE - 1);

    CHECK(HostSerial.Read() == -1);
    for(int i = 0; i < SERIAL_RX_BUFFER_SIZE + 10; i++)
    {
        HostSerial.OnReceive((uint8_t)i);
    }
    CHECK(HostSerial.Available() == SERIAL_RX_BUFFER_SIZE - 1);
    CHECK(HostSerial.Read() == 0);
    CHECK(HostSerial.Available() == SERIAL_RX_BUFFER_SIZE - 2);
}

static void TestSerialPortWritesAcrossWrap()
{
    ResetAll();
    int mismatches = 0;

    for(int length = 1; length < SERIAL_TX_BUFFER_SIZE; length++)
    {
        std::string chunk;
        for(int i = 0; i < length; i++)
        {
            chunk.push_back((char)('A' + (length + i) % 26));
        }

        if(!HostSerial.Write(chunk.data(), (uint8_t)length) || HostBoard::Drain(HostSerial) != chunk)
        {
            mismatches++;
        }
    }

    CHECK(mismatches == 0);
    CHECK(HostSerial.WriteSpace() == SERIAL_TX_BUFFER_SIZE - 1);

    std::string tooLong(SERIAL_TX_BUFFER_SIZE, 'x');
    CHECK(!HostSerial.Write(tooLong.data(), (uint8_t)SERIAL_TX_BUFFER_SIZE));
    CHECK(HostBoard::Drain(HostSerial).empty());
}

static std::string Decimal(int16_t value, uint8_t minimumDigits)
{
    char text[8];
    return std::string(text, WriteDecimal(text, value, minimumDigits));
}

static std::string Hex(uint16_t value, uint8_t digits)
{
    char text[8];
    return std::string(text, WriteHex(text, value, digits));
}

static std::string Hundredths(float value)
{
    char text[16];
    return std::string(text, WriteHundredths(text, value));
}

static std::string ReferenceHundredths(float value)
{
    uint32_t hundredths = 999999;
    std::string text;
    if(!std::isnan(value))
    {
        if(value < 0.0f)
        {
            text.push_back('-');
        }

        double rounded = std::nearbyint(std::fabs((double)value) * 100.0);
        hundredths = rounded >= 999999.0 ? 999999u : (uint32_t)rounded;
    }

    text += std::to_string(hundredths / 100);
    text.push_back('.');
    text.push_back((char)('0' + hundredths / 10 % 10));
    text.push_back((char)('0' + hundredths % 10));
    return text;
}

static float FloatFromBits(uint32_t bits)
{
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void TestTextFormatIntegers()
{
    CHECK_TEXT(Decimal(0, 1), "0");
    CHECK_TEXT(Decimal(7, 2), "07");
    CHECK_TEXT(Decimal(123, 2), "123");
    CHECK_TEXT(Decimal(-1, 1), "-1");
    CHECK_TEXT(Decimal(0, 5), "00000");
    CHECK_TEXT(Hex(0x00C0, 4), "00C0");
    CHECK_TEXT(Hex(0x9D, 2), "9D");
    CHECK_TEXT(Hex(0x1234, 2), "34");

    int mismatches = 0;
    for(int value = -32768; value <= 32767; value++)
    {
        char expected[16];
        snprintf(expected, sizeof(expected), value < 0 ? "%d" : "%02d", value);
        if(Decimal((int16_t)value, value < 0 ? 1 : 2) != expected)
        {
            mismatches++;
        }
    }

    for(unsigned int value = 0; value <= 0xFFFF; value++)
    {
        char expected[16];
        snprintf(expected, sizeof(expected), "%04X", value);
        if(Hex((uint16_t)value, 4) != expected)
        {
            mismatches++;
        }
    }

    CHECK(mismatches == 0);
}

static void TestTextFormatHundredthsMatchesReference()
{
    CHECK_TEXT(Hundredths(0.125f), "0.12");
    CHECK_TEXT(Hundredths(0.375f), "0.38");
    CHECK_TEXT(Hundredths(20.0f), "20.00");
    CHECK_TEXT(Hundredths(-1.25f), "-1.25");
    CHECK_TEXT(Hundredths(-0.0f), "0.00");
    CHECK_TEXT(Hundredths(-0.001f), "-0.00");
    CHECK_TEXT(Hundredths(1.0e6f), "9999.99");
    CHECK_TEXT(Hundredths(std::numeric_limits<float>::quiet_NaN()), "9999.99");
    CHECK_TEXT(Hundredths(std::copysign(std::numeric_limits<float>::quiet_NaN(), -1.0f)), "9999.99");
    CHECK_TEXT(Hundredths(-std::numeric_limits<float>::infinity()), "-9999.99");

    int mismatches = 0;
    auto compare = [&mismatches](uint32_t bits)
    {
        for(uint32_t sign = 0; sign <= 1; sign++)
        {
            float value = FloatFromBits(sign << 31 | bits);
            std::string actual = Hundredths(value);
            std::string expected = ReferenceHundredths(value);
            if(actual != expected)
            {
                if(mismatches < 5)
                {
                    printf("  %.9g: %s vs %s\n", (double)value, actual.c_str(), expected.c_str());
                }
                mismatches++;
            }
        }
    };

    for(uint32_t exponent = 0; exponent < 256; exponent++)
    {
        for(uint32_t mantissa = 0; mantissa <= 0x7FFFFF; mantissa += 65537)
        {
            compare(exponent << 23 | mantissa);
        }
        compare(exponent << 23 | 0x7FFFFF);
    }

    for(uint32_t shift = 3; shift <= 26; shift++)
    {
        uint32_t lowest = (1u << (26 - shift)) | 1u;
        uint32_t highest = (1u << (27 - shift)) - 1u;
        const uint32_t odds[] = { lowest, lowest + 2u, highest };
        for(uint32_t odd : odds)
        {
            uint32_t tie = odd << (shift - 3);
            for(uint32_t mantissa = tie - 1; odd <= highest && mantissa <= tie + 1; mantissa++)
            {
                if(mantissa >= 0x800000u && mantissa <= 0xFFFFFFu)
                {
                    compare((150u - shift) << 23 | (mantissa - 0x800000u));
                }
            }
        }
    }

    CHECK(mismatches == 0);
}

static void TestTextFormatMatchesPrintfForAnalogValues()
{
    int mismatches = 0;
    auto compare = [&mismatches](float value)
    {
        char expected[16];
        snprintf(expected, sizeof(expected), "%.2f", (double)value);
        std::string actual = Hundredths(value);
        if(actual != expected)
        {
            if(mismatches < 5)
            {
                printf("  %.9g: %s vs %s\n", (double)value, actual.c_str(), expected);
            }
            mismatches++;
        }
    };

    for(int code = 0; code <= 1023; code++)
    {
        compare(code * ANALOG_INPUT_FULL_SCALE / ANALOG_INPUT_RESOLUTION);
    }

    for(unsigned long raw = 0; raw <= 0xFFFF; raw++)
    {
        compare((raw * ANALOG_INPUT_FULL_SCALE) / 65535.0f);
    }

    CHECK(mismatches == 0);
}

static void TestRemoteVersion()
{
    ResetAll();
    RemoteServer server(HostSerial);

    std::string request = Frame("GVER");
    CHECK(request == std::string("\x02" "GVER" "\x03"));
    CHECK_TEXT(Exchange(server, "GVER"), Frame(std::string("GVER ") + VERSION));
}

static void TestRemoteStatusMatchesSpecificationExample()
{
    ResetAll();
    RemoteServer server(HostSerial);
    global._controlMode = ControlMode::CM_Manual;
    global._digitalOutputs[6] = true;
    global._digitalOutputs[7] = true;

    CHECK_TEXT(Exchange(server, "GSTA"), Frame("GSTA 2,2,0000,00C0,0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00"));
}

static void TestRemoteDetailedStatusMatchesSpecificationExample()
{
    ResetAll();
    RemoteServer server(HostSerial);
    global._controlMode = ControlMode::CM_Auto;
    global._digitalInputs[2] = true;
    global._digitalOutputs[0] = true;
    global._analogInputsKisan[0] = 4.0f;

    std::string expected = "GDST 2,1,0004,0001,0.00,0.00,0.00,0.00,4.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00";
    CHECK_TEXT(Exchange(server, "GDST"), Frame(expected));
}

static void TestRemoteStatusBitmaps()
{
    ResetAll();
    RemoteServer server(HostSerial);
    global._digitalInputs[15] = true;
    global._digitalInputs[0] = true;
    global._kisanOutputs[0] = true;
    global._kisanOutputs[7] = true;

    std::vector<std::string> fields = Fields(Exchange(server, "GSTA"));
    CHECK(fields.size() == 16);
    CHECK_TEXT(fields[2], "8001");
    CHECK_TEXT(fields[3], "8100");
}

static void TestRemoteOxygenMatchesSpecificationExample()
{
    ResetAll();
    RemoteServer server(HostSerial);
    global._oxygenConcentration = 19.75f;
    global._analogInputsKisan[OXYGEN_SENSOR_INPUT_INDEX] = 16.0f;

    CHECK_TEXT(Exchange(server, "OXYG"), Frame("OXYG 20 16 "));
}

static void TestRemoteSetDigitalOutput()
{
    ResetAll();
    RemoteServer server(HostSerial);
    global._controlMode = ControlMode::CM_Auto;

    CHECK_TEXT(Exchange(server, "SSDO 3,1"), Frame("SSDO"));
    CHECK(HostBoard::Outputs == 0x08);
    CHECK_TEXT(Exchange(server, "SSDO 3,0"), Frame("SSDO"));
    CHECK(HostBoard::Outputs == 0x00);

    CHECK_TEXT(Exchange(server, "SSDO 9,1"), Frame("SSDO"));
    CHECK(global._kisanOutputs[1]);

    CHECK_TEXT(Exchange(server, "SSDO 16,1"), Frame("SSDO E0008"));
    CHECK_TEXT(Exchange(server, "SSDO 1,2"), Frame("SSDO E0008"));
    CHECK_TEXT(Exchange(server, "SSDO 1"), Frame("SSDO E0006"));
    CHECK_TEXT(Exchange(server, "SSDO 1,a"), Frame("SSDO E0007"));

    global._controlMode = ControlMode::CM_Manual;
    CHECK_TEXT(Exchange(server, "SSDO 1,1"), Frame("SSDO E0009"));
    CHECK_TEXT(Exchange(server, "SSDO 1"), Frame("SSDO E0009"));
}

static void TestRemoteToggleDigitalOutput()
{
    ResetAll();
    RemoteServer server(HostSerial);
    global._controlMode = ControlMode::CM_Auto;

    CHECK_TEXT(Exchange(server, "TSDO 3"), Frame("TSDO"));
    CHECK(HostBoard::Outputs == 0x08);

    global._digitalOutputs[3] = true;
    CHECK_TEXT(Exchange(server, "TSDO 3"), Frame("TSDO"));
    CHECK(HostBoard::Outputs == 0x00);

    CHECK_TEXT(Exchange(server, "TSDO 10"), Frame("TSDO"));
    CHECK(global._kisanOutputs[2]);

    CHECK_TEXT(Exchange(server, "TSDO 99"), Frame("TSDO E0008"));
    CHECK_TEXT(Exchange(server, "TSDO 1a"), Frame("TSDO E0007"));
    CHECK_TEXT(Exchange(server, "TSDO 1,2"), Frame("TSDO E0006"));
}

static void TestRemoteSetAnalogOutput()
{
    ResetAll();
    RemoteServer server(HostSerial);

    CHECK_TEXT(Exchange(server, "SSAO 0,12000"), Frame("SSAO"));
    CHECK(global._analogOutputsKisanTargets[0] == 12000.0f);
    CHECK_TEXT(Exchange(server, "SSAO 3,20000"), Frame("SSAO"));
    CHECK_TEXT(Exchange(server, "SSAO 4,1"), Frame("SSAO E0008"));
    CHECK_TEXT(Exchange(server, "SSAO 0,20001"), Frame("SSAO E0008"));
    CHECK_TEXT(Exchange(server, "SSAO 0"), Frame("SSAO E0006"));
}

static void TestRemoteArgumentNumbers()
{
    ResetAll();
    RemoteServer server(HostSerial);
    global._controlMode = ControlMode::CM_Auto;

    CHECK_TEXT(Exchange(server, "SSDO 0003,1"), Frame("SSDO"));
    CHECK(HostBoard::Outputs == 0x08);
    CHECK_TEXT(Exchange(server, "SSDO 5,1,"), Frame("SSDO"));
    CHECK(HostBoard::Outputs == 0x28);

    CHECK_TEXT(Exchange(server, "SSDO ,0"), Frame("SSDO E0007"));
    CHECK_TEXT(Exchange(server, "SSDO 3,"), Frame("SSDO E0006"));
    CHECK_TEXT(Exchange(server, "SSDO 99,a"), Frame("SSDO E0007"));
    CHECK_TEXT(Exchange(server, "SSDO 65539,0"), Frame("SSDO E0008"));
    CHECK_TEXT(Exchange(server, "SSDO 3,65536"), Frame("SSDO E0008"));
    CHECK_TEXT(Exchange(server, "SSDO 9999999,0"), Frame("SSDO E0008"));
    CHECK_TEXT(Exchange(server, "TSDO 65539"), Frame("TSDO E0008"));
    CHECK(HostBoard::Outputs == 0x28);

    CHECK_TEXT(Exchange(server, "SSAO ,100"), Frame("SSAO E0007"));
    CHECK_TEXT(Exchange(server, "SSAO 65536,100"), Frame("SSAO E0008"));
    CHECK_TEXT(Exchange(server, "SSAO 0,65535"), Frame("SSAO E0008"));
    CHECK(global._analogOutputsKisanTargets[0] == 0.0f);
}

static void TestRemoteParserErrors()
{
    ResetAll();
    RemoteServer server(HostSerial);
    global._controlMode = ControlMode::CM_Auto;

    CHECK_TEXT(Exchange(server, "ABCD"), Frame("ABCD E0005"));
    CHECK_TEXT(Exchange(server, "ABCDEFGH"), Frame("ABCDEFG E0001"));
    CHECK_TEXT(Exchange(server, "SSDO 1,2,3,4,5,6,7,8,9"), Frame("SSDO E0002"));
    CHECK_TEXT(Exchange(server, "SSDO 12345678,1"), Frame("SSDO E0003"));
    CHECK_TEXT(Exchange(server, "SSDO "), Frame("SSDO E0004"));
}

static void TestRemoteRecoversFromReceiveOverflow()
{
    ResetAll();
    RemoteServer server(HostSerial);

    HostBoard::Feed(HostSerial, std::string(40, 'x'));
    server.Process();
    CHECK(HostBoard::Drain(HostSerial).empty());

    CHECK_TEXT(Exchange(server, "GVER"), Frame(std::string("GVER ") + VERSION));
}

static void TestRemoteIgnoresBytesOutsideFrames()
{
    ResetAll();
    RemoteServer server(HostSerial);

    HostBoard::Feed(HostSerial, std::string(1, ETX) + "xyz" + Frame("GVER"));
    server.Process();
    CHECK_TEXT(HostBoard::Drain(HostSerial), Frame(std::string("GVER ") + VERSION));
}

static void TestRemoteRestartsFrameOnNewStx()
{
    ResetAll();
    RemoteServer server(HostSerial);

    HostBoard::Feed(HostSerial, std::string(1, STX) + "GV" + Frame("OXYG"));
    server.Process();
    CHECK_TEXT(HostBoard::Drain(HostSerial), Frame("OXYG 0 0 "));
}

static void TestRemoteAnswersQueuedRequestsInOrder()
{
    ResetAll();
    RemoteServer server(HostSerial);

    HostBoard::Feed(HostSerial, Frame("GVER") + "zz" + Frame("ABCD"));
    server.Process();
    CHECK_TEXT(HostBoard::Drain(HostSerial), Frame(std::string("GVER ") + VERSION));
    server.Process();
    CHECK_TEXT(HostBoard::Drain(HostSerial), Frame("ABCD E0005"));
    server.Process();
    CHECK(HostBoard::Drain(HostSerial).empty());
}

static void TestRemoteFrameLengthLimit()
{
    ResetAll();
    RemoteServer server(HostSerial);

    HostBoard::Feed(HostSerial, Frame(std::string(REMOTE_RECEIVE_BUFFER_SIZE - 2, 'A')));
    server.Process();
    CHECK_TEXT(HostBoard::Drain(HostSerial), Frame("AAAAAAA E0001"));

    HostBoard::Feed(HostSerial, Frame(std::string(REMOTE_RECEIVE_BUFFER_SIZE - 1, 'A')) + Frame("GVER"));
    server.Process();
    CHECK_TEXT(HostBoard::Drain(HostSerial), Frame(std::string("GVER ") + VERSION));
}

static void TestRemoteWaitsForTransmitSpace()
{
    ResetAll();
    RemoteServer server(HostSerial);

    for(int i = 0; i < 20; i++)
    {
        HostSerial.Write((uint8_t)'.');
    }

    HostBoard::Feed(HostSerial, Frame("GVER"));
    server.Process();
    CHECK_TEXT(HostBoard::Drain(HostSerial), std::string(20, '.'));

    server.Process();
    CHECK_TEXT(HostBoard::Drain(HostSerial), Frame(std::string("GVER ") + VERSION));
}

static void TestKiSANAnalogInputModule()
{
    ResetAll();
    KiSAN_KM6015 module(FieldbusSerial);

    CHECK(!module.Process());
    CHECK_TEXT(HostBoard::Drain(FieldbusSerial), "#01G82089D\r");

    HostBoard::Feed(FieldbusSerial, "noise*01,3333,0000,0000,0000,0000,0000,0000,FFFF,AB\r");
    CHECK(!module.Process());
    CHECK(module.IsConnected());
    CHECK(!module.Process());
    CHECK(global._analogInputsKisan[0] > 3.999f && global._analogInputsKisan[0] < 4.001f);
    CHECK(global._analogInputsKisan[7] == 20.0f);

    Advance(499);
    CHECK(!module.Process());
    Advance(1);
    CHECK(module.Process());
}

static void TestKiSANIgnoresExtraFieldsAndBadFrames()
{
    ResetAll();
    KiSAN_KM6015 module(FieldbusSerial);
    global._analogInputsKisan[0] = 1.0f;

    module.Process();
    HostBoard::Drain(FieldbusSerial);

    HostBoard::Feed(FieldbusSerial, "*1\r");
    CHECK(!module.Process());
    CHECK(global._analogInputsKisan[0] == 1.0f);

    std::string noisy = "*" + std::string(80, '7') + "*01,FFFF,FFFF,FFFF,FFFF,FFFF,FFFF,FFFF,FFFF,FFFF,FFFF,AB\r";
    for(size_t offset = 0; offset < noisy.size(); offset += 16)
    {
        HostBoard::Feed(FieldbusSerial, noisy.substr(offset, 16));
        CHECK(!module.Process());
    }
    CHECK(!module.Process());
    CHECK(global._analogInputsKisan[0] == 20.0f);
    CHECK(global._analogInputsKisan[7] == 20.0f);
}

static void TestKiSANTimeoutReleasesBus()
{
    ResetAll();
    KiSAN_KM6015 module(FieldbusSerial);

    module.Process();
    HostBoard::Feed(FieldbusSerial, "*01,0000,0000,0000,0000,0000,0000,0000,0000,AB\r");
    module.Process();
    module.Process();
    Advance(KISAN_POLL_INTERVAL_MILLISECONDS);
    CHECK(module.Process());
    CHECK(module.IsConnected());

    for(int attempt = 0; attempt <= KISAN_RETRY_COUNT; attempt++)
    {
        CHECK(!module.Process());
        Advance(KISAN_RECEIVE_TIMEOUT_MILLISECONDS - 1);
        CHECK(!module.Process());
        Advance(1);
        CHECK(module.Process());
    }

    CHECK(!module.IsConnected());
}

static void TestKiSANDigitalOutputFrames()
{
    ResetAll();
    KiSAN_KM6063 module(FieldbusSerial);

    global._kisanOutputs[6] = true;
    global._kisanOutputs[7] = true;
    module.Process();
    CHECK_TEXT(HostBoard::Drain(FieldbusSerial), "#02S0001,00C098\r");

    ResetAll();
    KiSAN_KM6063 idle(FieldbusSerial);
    idle.Process();
    CHECK_TEXT(HostBoard::Drain(FieldbusSerial), "#02S0001,000085\r");
}

static void TestKiSANAnalogOutputModule()
{
    ResetAll();
    KiSAN_KM6023 module(FieldbusSerial);
    global._analogInputs[0] = 4.0f;

    module.Process();
    CHECK_TEXT(HostBoard::Drain(FieldbusSerial), "#01S0204,0FA0,0000,0000,000074\r");

    HostBoard::Feed(FieldbusSerial, "*01,0FA0,0000,0000,4E20,AB\r");
    module.Process();
    module.Process();
    CHECK(global._analogOutputsKisan[0] == 4.0f);
    CHECK(global._analogOutputsKisan[3] == 20.0f);
}

static void SetInput(uint8_t index, bool state)
{
    uint16_t mask = (uint16_t)(1u << index);
    HostBoard::Inputs = state ? (uint16_t)(HostBoard::Inputs | mask) : (uint16_t)(HostBoard::Inputs & ~mask);
    global._digitalInputs[index] = state;
}

static void TestControlModeSelection()
{
    ResetAll();

    SetInput(0, true);
    ProcessControlMode();
    CHECK(global._controlMode == ControlMode::CM_Manual);

    SetInput(0, false);
    SetInput(1, false);
    ProcessControlMode();
    CHECK(global._controlMode == ControlMode::CM_Manual);

    HostBoard::Outputs = 0xFF;
    SetInput(1, true);
    ProcessControlMode();
    CHECK(global._controlMode == ControlMode::CM_Auto);
    CHECK(HostBoard::Outputs == 0x00);
}

static void TestEmergencyAlarmAndRelease()
{
    ResetAll();
    SetInput(1, true);
    ProcessControlMode();
    HostBoard::Outputs = 0x5F;
    global._digitalInputsClicked[3] = true;

    SetInput(EMERGENCY_INPUT_INDEX, true);
    ProcessControlMode();
    CHECK(global._controlMode == ControlMode::CM_Emergency);
    CHECK(HostBoard::Outputs == (1 << ALARM_OUTPUT_INDEX));
    CHECK(!global._digitalInputsClicked[3]);

    SetInput(EMERGENCY_INPUT_INDEX, false);
    SetInput(0, true);
    ProcessControlMode();
    CHECK(global._controlMode == ControlMode::CM_Manual);
    CHECK(HostBoard::Outputs == 0x00);
}

static void TestPairedOutputsFollowClicksInManualMode()
{
    ResetAll();
    global._controlMode = ControlMode::CM_Manual;
    global._digitalInputsClicked[2] = true;
    global._digitalInputsClicked[6] = true;
    global._digitalInputsClicked[7] = true;
    global._digitalInputsClicked[10] = true;

    ProcessPairedIOControl();
    CHECK(HostBoard::Outputs == 0x11);
    CHECK(global._kisanOutputs[0]);
    CHECK(!global._kisanOutputs[1]);
    CHECK(global._kisanOutputs[3]);

    global._controlMode = ControlMode::CM_Auto;
    global._digitalInputsClicked[2] = false;
    ProcessPairedIOControl();
    CHECK(HostBoard::Outputs == 0x11);
}

static void TestInputDebounceAndClicks()
{
    ResetAll();
    IOManager manager;
    global._controlMode = ControlMode::CM_Manual;

    HostBoard::Inputs = 0x0004;
    manager.Process();
    Advance(50);
    manager.Process();
    CHECK(!global._digitalInputs[2]);
    Advance(50);
    manager.Process();
    CHECK(global._digitalInputs[2]);
    CHECK(!global._digitalInputsClicked[2]);

    HostBoard::Inputs = 0x0000;
    manager.Process();
    Advance(100);
    manager.Process();
    CHECK(!global._digitalInputs[2]);
    CHECK(global._digitalInputsClicked[2]);

    HostBoard::Inputs = 0x0004;
    manager.Process();
    Advance(40);
    HostBoard::Inputs = 0x0000;
    Advance(60);
    manager.Process();
    CHECK(!global._digitalInputs[2]);
}

static void TestOutputReadbackAndAnalogScaling()
{
    ResetAll();
    IOManager manager;
    HostBoard::Outputs = 0x81;
    HostBoard::Analog[0] = 1023;
    HostBoard::Analog[3] = 512;

    manager.Process();
    CHECK(global._digitalOutputs[0]);
    CHECK(!global._digitalOutputs[1]);
    CHECK(global._digitalOutputs[7]);
    CHECK(global._analogInputs[0] == 20.0f);
    CHECK(global._analogInputs[3] > 10.009f && global._analogInputs[3] < 10.011f);
}

static void TestStatusLedBlinksEverySecond()
{
    ResetAll();
    BuiltInLED led;
    led.Initialize(BUILTIN_LED_INTERVAL_MILLISECONDS);

    led.Blink();
    CHECK(HostBoard::LedToggles == 0);
    Advance(1000);
    led.Blink();
    CHECK(HostBoard::LedToggles == 1);
    led.Blink();
    CHECK(HostBoard::LedToggles == 1);
    Advance(1000);
    led.Blink();
    CHECK(HostBoard::LedToggles == 2);
}

static void TestExternalFanControl()
{
    ResetAll();
    ExternalFanControl();
    CHECK(HostBoard::Outputs == 0x00);

    global._analogInputsKisan[OXYGEN_SENSOR_INPUT_INDEX] = 20.0f;
    Advance(1000);
    ExternalFanControl();
    CHECK(global._oxygenConcentration == 25.0f);
    CHECK(HostBoard::Outputs == 0x00);

    global._digitalOutputs[MAIN_VALVE_OUTPUT_INDEX] = true;
    Advance(1000);
    ExternalFanControl();
    CHECK(HostBoard::Outputs == 0xC0);

    global._digitalOutputs[MAIN_VALVE_OUTPUT_INDEX] = false;
    global._analogInputsKisan[OXYGEN_SENSOR_INPUT_INDEX] = 16.0f;
    Advance(1000);
    ExternalFanControl();
    CHECK(HostBoard::Outputs == 0xC0);

    global._analogInputsKisan[OXYGEN_SENSOR_INPUT_INDEX] = 20.0f;
    Advance(1000);
    ExternalFanControl();
    CHECK(HostBoard::Outputs == 0x00);
}

typedef void (*TestFunction)();

struct TestCase
{
    const char *Name;
    TestFunction Function;
};

static const TestCase TESTS[] =
{
    { "ChecksumOfDocumentedFrames", TestChecksumOfDocumentedFrames },
    { "TimerSurvivesMillisWraparound", TestTimerSurvivesMillisWraparound },
    { "SerialPortBuffersAndLimits", TestSerialPortBuffersAndLimits },
    { "SerialPortWritesAcrossWrap", TestSerialPortWritesAcrossWrap },
    { "TextFormatIntegers", TestTextFormatIntegers },
    { "TextFormatHundredthsMatchesReference", TestTextFormatHundredthsMatchesReference },
    { "TextFormatMatchesPrintfForAnalogValues", TestTextFormatMatchesPrintfForAnalogValues },
    { "RemoteVersion", TestRemoteVersion },
    { "RemoteStatusMatchesSpecificationExample", TestRemoteStatusMatchesSpecificationExample },
    { "RemoteDetailedStatusMatchesSpecificationExample", TestRemoteDetailedStatusMatchesSpecificationExample },
    { "RemoteStatusBitmaps", TestRemoteStatusBitmaps },
    { "RemoteOxygenMatchesSpecificationExample", TestRemoteOxygenMatchesSpecificationExample },
    { "RemoteSetDigitalOutput", TestRemoteSetDigitalOutput },
    { "RemoteToggleDigitalOutput", TestRemoteToggleDigitalOutput },
    { "RemoteSetAnalogOutput", TestRemoteSetAnalogOutput },
    { "RemoteArgumentNumbers", TestRemoteArgumentNumbers },
    { "RemoteParserErrors", TestRemoteParserErrors },
    { "RemoteRecoversFromReceiveOverflow", TestRemoteRecoversFromReceiveOverflow },
    { "RemoteIgnoresBytesOutsideFrames", TestRemoteIgnoresBytesOutsideFrames },
    { "RemoteRestartsFrameOnNewStx", TestRemoteRestartsFrameOnNewStx },
    { "RemoteAnswersQueuedRequestsInOrder", TestRemoteAnswersQueuedRequestsInOrder },
    { "RemoteFrameLengthLimit", TestRemoteFrameLengthLimit },
    { "RemoteWaitsForTransmitSpace", TestRemoteWaitsForTransmitSpace },
    { "KiSANAnalogInputModule", TestKiSANAnalogInputModule },
    { "KiSANIgnoresExtraFieldsAndBadFrames", TestKiSANIgnoresExtraFieldsAndBadFrames },
    { "KiSANTimeoutReleasesBus", TestKiSANTimeoutReleasesBus },
    { "KiSANDigitalOutputFrames", TestKiSANDigitalOutputFrames },
    { "KiSANAnalogOutputModule", TestKiSANAnalogOutputModule },
    { "ControlModeSelection", TestControlModeSelection },
    { "EmergencyAlarmAndRelease", TestEmergencyAlarmAndRelease },
    { "PairedOutputsFollowClicksInManualMode", TestPairedOutputsFollowClicksInManualMode },
    { "InputDebounceAndClicks", TestInputDebounceAndClicks },
    { "OutputReadbackAndAnalogScaling", TestOutputReadbackAndAnalogScaling },
    { "StatusLedBlinksEverySecond", TestStatusLedBlinksEverySecond },
    { "ExternalFanControl", TestExternalFanControl },
};

int main()
{
    int failedTests = 0;

    for(const TestCase &test : TESTS)
    {
        int before = failures;
        std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
        test.Function();
        long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
        bool passed = failures == before;
        printf("%s %s %lld ms\n", passed ? "PASS" : "FAIL", test.Name, elapsed);
        if(!passed)
        {
            failedTests++;
        }
    }

    int total = (int)(sizeof(TESTS) / sizeof(TESTS[0]));
    printf("tests %d, passed %d, failed %d, checks %d\n", total, total - failedTests, failedTests, checks);
    return failedTests == 0 ? 0 : 1;
}
