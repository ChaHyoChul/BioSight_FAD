#include "Board.h"
#include "GlobalDefinition.h"
#include "SerialHardware.h"
#include "SerialPort.h"
#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/atomic.h>

#define TIMER0_PRESCALER (64UL)
#define TIMER0_TICKS_PER_MILLISECOND (F_CPU / TIMER0_PRESCALER / 1000UL)
#define ANALOG_CHANNEL_MASK (ANALOG_INPUT_COUNT - 1)
#define ADC_CONTROL (_BV(ADEN) | _BV(ADIE) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0))

static_assert(TIMER0_TICKS_PER_MILLISECOND > 0 && TIMER0_TICKS_PER_MILLISECOND <= 256, "Timer0 cannot produce a 1 ms tick at this clock");
static_assert((ANALOG_INPUT_COUNT & ANALOG_CHANNEL_MASK) == 0, "Analog channel count must be a power of two");

static volatile uint32_t milliseconds;
static volatile uint16_t analogValues[ANALOG_INPUT_COUNT];
static volatile uint8_t analogChannel;
static volatile uint8_t analogSamples;

static uint8_t Reverse(uint8_t value)
{
    value = (uint8_t)(((value & 0xF0) >> 4) | ((value & 0x0F) << 4));
    value = (uint8_t)(((value & 0xCC) >> 2) | ((value & 0x33) << 2));
    value = (uint8_t)(((value & 0xAA) >> 1) | ((value & 0x55) << 1));
    return value;
}

void Board::Initialize()
{
    DDRA = 0xFF;
    PORTA = 0x00;

    DDRB |= _BV(PB7);
    PORTB &= (uint8_t)~_BV(PB7);

    DDRC = 0x00;
    PORTC = 0x00;
    DDRD &= (uint8_t)~_BV(PD7);
    PORTD &= (uint8_t)~_BV(PD7);
    DDRG &= (uint8_t)~(_BV(PG0) | _BV(PG1) | _BV(PG2));
    PORTG &= (uint8_t)~(_BV(PG0) | _BV(PG1) | _BV(PG2));
    DDRL &= 0x0F;
    PORTL &= 0x0F;

    TCCR0A = _BV(WGM01);
    OCR0A = (uint8_t)(TIMER0_TICKS_PER_MILLISECOND - 1);
    TIMSK0 = _BV(OCIE0A);
    TCCR0B = _BV(CS01) | _BV(CS00);

    DIDR0 = (uint8_t)((1 << ANALOG_INPUT_COUNT) - 1);
    ADMUX = _BV(REFS0);
    ADCSRB = 0;
    ADCSRA = ADC_CONTROL;

    sei();
}

uint32_t Board::Millis()
{
    uint32_t value;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        value = milliseconds;
    }
    return value;
}

uint16_t Board::ReadDigitalInputs()
{
    uint8_t low = Reverse(PINC);
    uint8_t high = (uint8_t)(((PIND >> PD7) & 0x01) | (Reverse((uint8_t)(PING & 0x07)) >> 4) | (Reverse((uint8_t)(PINL & 0xF0)) << 4));
    return (uint16_t)((high << 8) | low);
}

uint8_t Board::ReadDigitalOutputs()
{
    return PORTA;
}

void Board::WriteDigitalOutput(uint8_t index, bool state)
{
    uint8_t mask = (uint8_t)(1 << index);
    if(state)
    {
        PORTA |= mask;
    }
    else
    {
        PORTA &= (uint8_t)~mask;
    }
}

void Board::WriteDigitalOutputs(uint8_t states)
{
    PORTA = states;
}

uint16_t Board::ReadAnalogInput(uint8_t channel)
{
    uint16_t value;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        value = analogValues[channel & ANALOG_CHANNEL_MASK];
    }
    return value;
}

uint8_t Board::AnalogSampleCount()
{
    return analogSamples;
}

void Board::ToggleStatusLed()
{
    PINB = _BV(PB7);
}

void SerialHardwareBegin(uint8_t index, uint32_t baudRate)
{
    uint16_t setting = (uint16_t)((F_CPU / 4 / baudRate - 1) / 2);

    switch(index)
    {
    case SERIAL_DEBUG_INDEX:
        UCSR0A = _BV(U2X0);
        UBRR0 = setting;
        UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
        UCSR0B = _BV(RXEN0) | _BV(TXEN0) | _BV(RXCIE0);
        break;

    case SERIAL_HOST_INDEX:
        UCSR1A = _BV(U2X1);
        UBRR1 = setting;
        UCSR1C = _BV(UCSZ11) | _BV(UCSZ10);
        UCSR1B = _BV(RXEN1) | _BV(TXEN1) | _BV(RXCIE1);
        break;

    case SERIAL_FIELDBUS_INDEX:
        UCSR3A = _BV(U2X3);
        UBRR3 = setting;
        UCSR3C = _BV(UCSZ31) | _BV(UCSZ30);
        UCSR3B = _BV(RXEN3) | _BV(TXEN3) | _BV(RXCIE3);
        break;

    default:
        break;
    }
}

void SerialHardwareStartTransmit(uint8_t index)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        switch(index)
        {
        case SERIAL_DEBUG_INDEX:
            UCSR0B |= _BV(UDRIE0);
            break;

        case SERIAL_HOST_INDEX:
            UCSR1B |= _BV(UDRIE1);
            break;

        case SERIAL_FIELDBUS_INDEX:
            UCSR3B |= _BV(UDRIE3);
            break;

        default:
            break;
        }
    }
}

ISR(TIMER0_COMPA_vect)
{
    milliseconds = milliseconds + 1;
    ADCSRA = ADC_CONTROL | _BV(ADSC);
}

ISR(ADC_vect)
{
    uint8_t channel = analogChannel;
    analogValues[channel] = ADC;

    channel = (uint8_t)((channel + 1) & ANALOG_CHANNEL_MASK);
    analogChannel = channel;
    ADMUX = (uint8_t)(_BV(REFS0) | channel);
    analogSamples = (uint8_t)(analogSamples + 1);
}

ISR(USART0_RX_vect)
{
    DebugSerial.OnReceive(UDR0);
}

ISR(USART0_UDRE_vect)
{
    uint8_t data;
    if(DebugSerial.TakeTransmit(&data))
    {
        UDR0 = data;
    }
    else
    {
        UCSR0B &= (uint8_t)~_BV(UDRIE0);
    }
}

ISR(USART1_RX_vect)
{
    HostSerial.OnReceive(UDR1);
}

ISR(USART1_UDRE_vect)
{
    uint8_t data;
    if(HostSerial.TakeTransmit(&data))
    {
        UDR1 = data;
    }
    else
    {
        UCSR1B &= (uint8_t)~_BV(UDRIE1);
    }
}

ISR(USART3_RX_vect)
{
    FieldbusSerial.OnReceive(UDR3);
}

ISR(USART3_UDRE_vect)
{
    uint8_t data;
    if(FieldbusSerial.TakeTransmit(&data))
    {
        UDR3 = data;
    }
    else
    {
        UCSR3B &= (uint8_t)~_BV(UDRIE3);
    }
}
