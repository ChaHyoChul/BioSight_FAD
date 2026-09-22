#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <simavr/sim_avr.h>
#include <simavr/sim_elf.h>
#include <simavr/sim_cycle_timers.h>
#include <simavr/avr_uart.h>
#include <simavr/avr_ioport.h>
#include <simavr/avr_adc.h>

#define CPU_FREQUENCY 16000000UL
#define CYCLES_PER_MILLISECOND (CPU_FREQUENCY / 1000UL)
#define SUPPLY_MILLIVOLTS 5000
#define DEBOUNCE_SETTLE_MILLISECONDS 250
#define RESPONSE_TIMEOUT_MILLISECONDS 400
#define LATENCY_SAMPLES 40

typedef struct
{
    char Port;
    int Bit;
} Pin;

static const Pin INPUT_PINS[16] =
{
    { 'C', 7 }, { 'C', 6 }, { 'C', 5 }, { 'C', 4 }, { 'C', 3 }, { 'C', 2 }, { 'C', 1 }, { 'C', 0 },
    { 'D', 7 }, { 'G', 2 }, { 'G', 1 }, { 'G', 0 }, { 'L', 7 }, { 'L', 6 }, { 'L', 5 }, { 'L', 4 }
};

static avr_t *avr;

static avr_flashaddr_t loopAddress;
static unsigned long loopHits;
static avr_cycle_count_t loopFirstCycle;
static avr_cycle_count_t loopLastCycle;
static avr_cycle_count_t loopMaxPeriod;
static avr_cycle_count_t windowMaxPeriod;
static int reportWindows;

static char hostOutput[4096];
static int hostLength;
static avr_cycle_count_t hostFirstByteCycle;

static char fieldbusLine[256];
static int fieldbusLength;
static int km6015Responds = 1;
static int km6015Requests;
static int km6063Requests;
static char km6063LastStates[8] = "none";
static char pendingReply[128];

static avr_cycle_count_t Milliseconds(double value)
{
    return (avr_cycle_count_t)(value * CYCLES_PER_MILLISECOND);
}

static void HostOutput(struct avr_irq_t *irq, uint32_t value, void *param)
{
    (void)irq;
    (void)param;

    if(hostLength == 0)
    {
        hostFirstByteCycle = avr->cycle;
    }

    if(hostLength < (int)sizeof(hostOutput) - 1)
    {
        hostOutput[hostLength++] = (char)value;
    }
}

static void Inject(char uart, const char *data, int length)
{
    avr_irq_t *irq = avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ(uart), UART_IRQ_INPUT);
    for(int i = 0; i < length; i++)
    {
        avr_raise_irq(irq, (uint8_t)data[i]);
    }
}

static avr_cycle_count_t SendPendingReply(struct avr_t *core, avr_cycle_count_t when, void *param)
{
    (void)core;
    (void)when;
    (void)param;
    Inject('3', pendingReply, (int)strlen(pendingReply));
    return 0;
}

static void ScheduleReply(const char *reply)
{
    snprintf(pendingReply, sizeof(pendingReply), "%s", reply);
    avr_cycle_timer_register(avr, Milliseconds(5), SendPendingReply, NULL);
}

static void FieldbusOutput(struct avr_irq_t *irq, uint32_t value, void *param)
{
    (void)irq;
    (void)param;

    if(fieldbusLength < (int)sizeof(fieldbusLine) - 1)
    {
        fieldbusLine[fieldbusLength++] = (char)value;
    }

    if(value != '\r')
    {
        return;
    }

    fieldbusLine[fieldbusLength] = 0;

    if(strncmp(fieldbusLine, "#01G8208", 8) == 0)
    {
        km6015Requests++;
        if(km6015Responds)
        {
            ScheduleReply("*01,3333,0000,0000,0000,0000,0000,0000,FFFF,AB\r");
        }
    }
    else if(strncmp(fieldbusLine, "#02S0001,", 9) == 0 && fieldbusLength >= 16)
    {
        km6063Requests++;
        memcpy(km6063LastStates, fieldbusLine + 9, 4);
        km6063LastStates[4] = 0;
        ScheduleReply("*02,0000,AB\r");
    }

    fieldbusLength = 0;
}

static void DisableConsoleFlags(char uart)
{
    uint32_t flags = 0;
    avr_ioctl(avr, AVR_IOCTL_UART_GET_FLAGS(uart), &flags);
    flags &= ~(uint32_t)(AVR_UART_FLAG_STDIO | AVR_UART_FLAG_POLL_SLEEP);
    avr_ioctl(avr, AVR_IOCTL_UART_SET_FLAGS(uart), &flags);
}

#define PROFILE_INTERVAL_CYCLES 61
#define PROFILE_SLOTS (256 * 1024 / 2)

static uint32_t *profileCounts;
static avr_cycle_count_t profileNextCycle;
static avr_cycle_count_t profileMinimumCycles;

static int Step(void)
{
    int state = avr_run(avr);
    if(profileCounts != NULL && avr->cycle >= profileNextCycle && avr->cycle - loopLastCycle >= profileMinimumCycles)
    {
        profileCounts[(avr->pc / 2) % PROFILE_SLOTS]++;
        profileNextCycle = avr->cycle + PROFILE_INTERVAL_CYCLES;
    }
    if(loopAddress != 0 && avr->pc == loopAddress)
    {
        if(loopHits == 0)
        {
            loopFirstCycle = avr->cycle;
        }
        else
        {
            avr_cycle_count_t period = avr->cycle - loopLastCycle;
            if(period > loopMaxPeriod)
            {
                loopMaxPeriod = period;
            }
            if(period > windowMaxPeriod)
            {
                windowMaxPeriod = period;
            }
        }
        loopLastCycle = avr->cycle;
        loopHits++;
    }
    return state;
}

static int RunFor(double milliseconds)
{
    avr_cycle_count_t target = avr->cycle + Milliseconds(milliseconds);
    while(avr->cycle < target)
    {
        int state = Step();
        if(state == cpu_Done || state == cpu_Crashed)
        {
            return -1;
        }
    }
    return 0;
}

static void SetInput(int index, int level)
{
    avr_raise_irq(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ(INPUT_PINS[index].Port), INPUT_PINS[index].Bit), (uint32_t)level);
}

static void SetAnalog(int channel, int code)
{
    uint32_t millivolts = (uint32_t)((code * SUPPLY_MILLIVOLTS + 1022) / 1023);
    avr_raise_irq(avr_io_getirq(avr, AVR_IOCTL_ADC_GETIRQ, ADC_IRQ_ADC0 + channel), millivolts);
}

static unsigned PortA(void)
{
    avr_ioport_state_t state;
    if(avr_ioctl(avr, AVR_IOCTL_IOPORT_GETSTATE('A'), &state) != 0)
    {
        return 0xFFFF;
    }
    return state.port;
}

static const char *Printable(const char *text, int length)
{
    static char buffer[8192];
    int out = 0;
    for(int i = 0; i < length && out < (int)sizeof(buffer) - 8; i++)
    {
        if(text[i] == 0x02)
        {
            out += snprintf(buffer + out, sizeof(buffer) - (size_t)out, "<STX>");
        }
        else if(text[i] == 0x03)
        {
            out += snprintf(buffer + out, sizeof(buffer) - (size_t)out, "<ETX>");
        }
        else
        {
            buffer[out++] = text[i];
        }
    }
    buffer[out] = 0;
    return buffer;
}

static const char *Request(const char *body, avr_cycle_count_t *latency)
{
    char frame[128];
    int length = snprintf(frame, sizeof(frame), "%c%s%c", 0x02, body, 0x03);

    hostLength = 0;
    hostFirstByteCycle = 0;

    avr_cycle_count_t start = avr->cycle;
    avr_cycle_count_t deadline = start + Milliseconds(RESPONSE_TIMEOUT_MILLISECONDS);
    Inject('1', frame, length);

    while(avr->cycle < deadline)
    {
        int state = Step();
        if(state == cpu_Done || state == cpu_Crashed)
        {
            break;
        }
        if(hostLength > 0 && hostOutput[hostLength - 1] == 0x03)
        {
            break;
        }
    }

    if(latency != NULL)
    {
        *latency = hostLength > 0 ? hostFirstByteCycle - start : 0;
    }

    return Printable(hostOutput, hostLength);
}

static void Transcript(const char *label, const char *body)
{
    avr_cycle_count_t latency = 0;
    windowMaxPeriod = 0;
    printf("RESP %s %s\n", label, Request(body, &latency));
    if(reportWindows)
    {
        printf("WINDOW %s %.4s %llu %llu\n", label, body, (unsigned long long)windowMaxPeriod, (unsigned long long)latency);
    }
}

static void ScenarioProtocol(void)
{
    Transcript("version", "GVER");
    Transcript("idle-status", "GSTA");
    Transcript("idle-detail", "GDST");
    Transcript("oxygen", "OXYG");
    Transcript("unknown", "ABCD");
    Transcript("e0001", "ABCDEFGH");
    Transcript("e0002", "SSDO 1,2,3,4,5,6,7,8,9");
    Transcript("e0003", "SSDO 12345678,1");
    Transcript("e0004", "SSDO ");
    Transcript("manual-ssdo", "SSDO 1,1");
    Transcript("ssao", "SSAO 0,12000");
    Transcript("ssao-range", "SSAO 4,1");
}

static void ScenarioInputs(void)
{
    for(int index = 0; index < 16; index++)
    {
        char label[32];

        SetInput(index, 1);
        RunFor(DEBOUNCE_SETTLE_MILLISECONDS);
        snprintf(label, sizeof(label), "input-%02d-on", index);
        Transcript(label, "GSTA");
        printf("PORT %s %02X\n", label, PortA());

        SetInput(index, 0);
        RunFor(DEBOUNCE_SETTLE_MILLISECONDS);
        snprintf(label, sizeof(label), "input-%02d-off", index);
        Transcript(label, "GSTA");
        printf("PORT %s %02X\n", label, PortA());
    }
}

static void ScenarioAutoOutputs(void)
{
    SetInput(1, 1);
    RunFor(DEBOUNCE_SETTLE_MILLISECONDS);
    Transcript("auto-mode", "GSTA");

    for(int index = 0; index < 8; index++)
    {
        char label[32];
        char body[32];

        snprintf(body, sizeof(body), "SSDO %d,1", index);
        snprintf(label, sizeof(label), "ssdo-%d-on", index);
        Transcript(label, body);
        RunFor(20);
        printf("PORT %s %02X\n", label, PortA());
    }

    Transcript("all-outputs", "GSTA");
    Transcript("toggle-3", "TSDO 3");
    RunFor(20);
    printf("PORT toggle-3 %02X\n", PortA());
    Transcript("toggle-range", "TSDO 99");
    Transcript("ssdo-format", "SSDO 1,a");
    Transcript("ssdo-count", "SSDO 1");
    Transcript("ssdo-empty", "SSDO ,0");
    Transcript("ssdo-wrap-index", "SSDO 65538,0");
    Transcript("ssdo-wrap-state", "SSDO 6,65536");
    Transcript("tsdo-wrap", "TSDO 65540");
    Transcript("ssao-wrap", "SSAO 65536,100");
    RunFor(20);
    printf("PORT argument-errors %02X\n", PortA());

    Transcript("kisan-output-9", "SSDO 9,1");
    RunFor(2500);
    printf("PORT kisan-frame %s\n", km6063LastStates);
    Transcript("kisan-status", "GDST");
}

static void ScenarioAnalog(void)
{
    for(int step = 0; step < 256; step++)
    {
        char label[32];
        for(int channel = 0; channel < 4; channel++)
        {
            SetAnalog(channel, step + channel * 256);
        }

        RunFor(10);
        snprintf(label, sizeof(label), "analog-%03d", step);
        Transcript(label, "GSTA");
    }
}

static void ScenarioEmergency(void)
{
    SetInput(15, 1);
    RunFor(DEBOUNCE_SETTLE_MILLISECONDS);
    printf("PORT emergency-on %02X\n", PortA());
    Transcript("emergency-status", "GSTA");
    Transcript("emergency-ssdo", "SSDO 1,1");

    SetInput(15, 0);
    RunFor(DEBOUNCE_SETTLE_MILLISECONDS);
    printf("PORT emergency-off %02X\n", PortA());
    Transcript("emergency-release", "GSTA");
}

static void MetricLatency(void)
{
    avr_cycle_count_t minimum = (avr_cycle_count_t)-1;
    avr_cycle_count_t maximum = 0;
    double total = 0;

    for(int sample = 0; sample < LATENCY_SAMPLES; sample++)
    {
        avr_cycle_count_t latency = 0;
        RunFor(0.137 * sample + 1.0);
        Request("GVER", &latency);
        if(latency < minimum)
        {
            minimum = latency;
        }
        if(latency > maximum)
        {
            maximum = latency;
        }
        total += (double)latency;
    }

    printf("METRIC latency-min-us %.1f\n", minimum * 1000000.0 / CPU_FREQUENCY);
    printf("METRIC latency-avg-us %.1f\n", total / LATENCY_SAMPLES * 1000000.0 / CPU_FREQUENCY);
    printf("METRIC latency-max-us %.1f\n", maximum * 1000000.0 / CPU_FREQUENCY);
    printf("METRIC latency-spread-us %.1f\n", (maximum - minimum) * 1000000.0 / CPU_FREQUENCY);
}

static double Microseconds(double cycles)
{
    return cycles * 1000000.0 / CPU_FREQUENCY;
}

static void MetricIdleLoop(void)
{
    if(loopAddress == 0)
    {
        return;
    }

    unsigned long hits = loopHits;
    avr_cycle_count_t start = avr->cycle;
    RunFor(2000);
    if(loopHits > hits)
    {
        printf("METRIC loop-idle-avg-us %.2f\n", Microseconds((double)(avr->cycle - start) / (double)(loopHits - hits)));
    }
}

static void MetricDeadModule(void)
{
    km6015Responds = 0;
    RunFor(1500);
    int before = km6063Requests;
    RunFor(10000);
    printf("METRIC km6063-polls-in-10s-with-km6015-dead %d\n", km6063Requests - before);
    km6015Responds = 1;
}

int main(int argc, char **argv)
{
    if(argc < 2)
    {
        fprintf(stderr, "usage: %s <firmware.elf> [loop-entry-address-hex]\n", argv[0]);
        return 2;
    }

    if(argc >= 3)
    {
        loopAddress = (avr_flashaddr_t)strtoul(argv[2], NULL, 16);
    }

    reportWindows = getenv("FAD_WINDOWS") != NULL;
    if(getenv("FAD_PROFILE") != NULL)
    {
        profileMinimumCycles = strtoull(getenv("FAD_PROFILE"), NULL, 10);
        profileCounts = calloc(PROFILE_SLOTS, sizeof(uint32_t));
    }

    elf_firmware_t firmware;
    memset(&firmware, 0, sizeof(firmware));
    if(elf_read_firmware(argv[1], &firmware) != 0)
    {
        fprintf(stderr, "cannot read firmware: %s\n", argv[1]);
        return 2;
    }

    avr = avr_make_mcu_by_name("atmega2560");
    if(avr == NULL)
    {
        fprintf(stderr, "simavr has no atmega2560 core\n");
        return 2;
    }

    avr_init(avr);
    avr->frequency = CPU_FREQUENCY;
    avr->vcc = SUPPLY_MILLIVOLTS;
    avr->avcc = SUPPLY_MILLIVOLTS;
    avr->aref = SUPPLY_MILLIVOLTS;
    avr_load_firmware(avr, &firmware);

    DisableConsoleFlags('0');
    DisableConsoleFlags('1');
    DisableConsoleFlags('3');
    avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('1'), UART_IRQ_OUTPUT), HostOutput, NULL);
    avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('3'), UART_IRQ_OUTPUT), FieldbusOutput, NULL);

    for(int index = 0; index < 16; index++)
    {
        SetInput(index, 0);
    }
    for(int channel = 0; channel < 4; channel++)
    {
        SetAnalog(channel, 0);
    }

    if(RunFor(200) != 0)
    {
        fprintf(stderr, "firmware stopped during start-up\n");
        return 1;
    }

    MetricIdleLoop();
    ScenarioProtocol();
    ScenarioInputs();
    ScenarioAutoOutputs();
    ScenarioAnalog();
    ScenarioEmergency();
    MetricLatency();
    MetricDeadModule();

    printf("METRIC km6015-requests %d\n", km6015Requests);
    printf("METRIC km6063-requests %d\n", km6063Requests);
    printf("METRIC simulated-seconds %.1f\n", (double)avr->cycle / CPU_FREQUENCY);
    if(loopHits > 1)
    {
        printf("METRIC loop-overall-avg-us %.2f\n", Microseconds((double)(loopLastCycle - loopFirstCycle) / (double)(loopHits - 1)));
        printf("METRIC loop-max-us %.2f\n", Microseconds((double)loopMaxPeriod));
    }

    if(profileCounts != NULL)
    {
        for(uint32_t slot = 0; slot < PROFILE_SLOTS; slot++)
        {
            if(profileCounts[slot] != 0)
            {
                printf("PROFILE %05x %u\n", (unsigned)(slot * 2), profileCounts[slot]);
            }
        }
        free(profileCounts);
    }
    return 0;
}
