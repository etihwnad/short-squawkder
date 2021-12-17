/*
 * Short Squawker
 *
 * Dan White <dan.white@valpo.edu>
 *
 * Find small resistances.
 * 
 */

#include <stdint.h>

// from AVR-GCC toolchain
#include <util/delay.h>
#include <avr/cpufunc.h>



#define BUTTON_PIN (1 << PB2)
#define OUTPUT_PIN (1 << PB1)
#define DEBUG_PIN (1 << PB0)

#define BUTTON_HOLD_TIMEOUT 400


#define TIMER0_MAX 200
#define PHASE_INC_MIN 1024


// This must be a power of 2
#define ADC_AVERAGE_GAIN 16
#define MAX_ACTIVE_INPUT 1023 * ADC_AVERAGE_GAIN


// *_PERIOD should be a power of 2
#define ADC_PERIOD 8
#define INPUT_PERIOD 16
#define ALTERNATE_PERIOD 2048

// offsets so there is only one major task
// for any given timer tick % (_PERIOD)
#define ADC_TICK_OFFSET   0
#define INPUT_TICK_OFFSET 1




volatile uint16_t average = 0;
volatile uint16_t dco_phase = 0;
volatile uint16_t phase_increment = 2000;
volatile uint16_t zero_reference = 0;
volatile uint8_t alternate_tone = 0;



/*
 * xdelay
 * 
 * Busy loop delay.
 * 
 *  input:
 *    uint16_t count -- loop count
 *    
 *  output:
 *    none
 */
void xdelay(uint16_t count)
{
  volatile uint8_t temp;

  for (uint16_t i=0; i<count; i++) {
    temp++;
  }
}




void timer_setup()
{
  const uint8_t TIMER0_PRESCALER_DIV64 = 0x03;
  const uint8_t TIMER0_PRESCALER_DIV8 = 0x02;
  const uint8_t TIMER0_PRESCALER_DIV1 = 0x01;

  /*
   * Timer0 setup
   */
  GTCCR = 0x00; //default
  TCCR0A = (0x2 << WGM00);
  
  TCCR0B = TIMER0_PRESCALER_DIV8;

  OCR0A = TIMER0_MAX;

  // interrupt on OCR0A match
  
  // enable overflow interrupt
  TIMSK = 0x00;  // default
  TIMSK |= (1 << OCIE0A);

  /*
   * Timer1 setup
   */
   TCCR1 = (1 << CTC1)  // clear timer on OCR1C match
           + (1 << PWM1A)   // PWM mode for OCR1A
           + (0b10 << COM1A0)  // both OC1A and OC1Ainv active
           + (0x01);  // CS1[3:0] clock select

    // longest PWM period
    OCR1C = 0xff;

   /* 12.2.1 Initialization for Async Mode:
      * enable PLL
      * wait 100us
      * poll PLOCK for 1
      * set PCKE */
   PLLCSR |= (1 << PLLE);
   _delay_us(100);
   while ( !(PLLCSR & PLOCK) );
   PLLCSR |= (1 << PCKE);
}


void adc_setup() {
  const uint8_t ADC_REF_INTERNAL_1V1 = (0 << REFS2) +
                                       (1 << REFS1) +
                                       (0 << REFS0);
  const uint8_t ADC_REF_VCC = (0 << REFS2) +
                              (0 << REFS1) +
                              (0 << REFS0);

  const uint8_t ADC_MUX_44_x1 = 0x04;
  const uint8_t ADC_MUX_44_x20 = 0x05;
  const uint8_t ADC_MUX_43_x1 = 0x06;
  const uint8_t ADC_MUX_43_x20 = 0x07;

  ADMUX = ADC_REF_INTERNAL_1V1 + 
          (0 << ADLAR) +
          ADC_MUX_43_x20;
          
  // div 64 from system clock
  // f_cpu 8 MHz / 64 => 125 kHz
  const uint8_t ADC_PRESCALER = 0x06;

  ADCSRA = (1 << ADEN) + 
           (1 << ADSC) +
           (0 << ADATE) +
           (0 << ADIE) +
           ADC_PRESCALER;

  // trigger conversion on timer0 overflow
  ADCSRB = 0x04;

  // disable digital port buffers on 
  DIDR0 = (1 << ADC2D) | (1 << ADC3D);
}



void pin_setup()
{
  DDRB |= OUTPUT_PIN;
  DDRB |= DEBUG_PIN;

  PORTB |= BUTTON_PIN;
}




void set_zero_reference(uint16_t value)
{
  zero_reference = value;
  debug_value(zero_reference);
}



void toggle_alternate_tone(void)
{
  if (alternate_tone) {
    alternate_tone = 0;
  }
  else {
    alternate_tone = 1;
  }
}





enum SM_input_states { button_up, button_down, timeout } SM_input_state;
void SM_input()
{
  static int SM_input_state = button_up;
  static unsigned int count = 0;

  uint8_t button = PINB & BUTTON_PIN;
  
  // transitions
  switch (SM_input_state) {
    case button_up:
      if (button) {
        SM_input_state = button_up;
      } else {
        SM_input_state = button_down;
      }
      break;
      
    case button_down:
      if (count > BUTTON_HOLD_TIMEOUT) {
        SM_input_state = timeout;
      }
      else if (button) {
        SM_input_state = button_up;
      } else {
        SM_input_state = button_down;
      }
      break;

    case timeout:
      SM_input_state = button_up;
      break;

    default:
      SM_input_state = button_up;
      break;
  }

  // actions
  switch (SM_input_state) {
    case button_up:
      if (count >= 1) {
        toggle_alternate_tone();
        count = 0;
      }
      break;
      
    case button_down:
      count++;
      break;

    case timeout:
      set_zero_reference(average);
      count = 0;
      break;

    default:
      break;
  } 
}












uint16_t adc_average(uint16_t value)
{
  average = value + (average - average / ADC_AVERAGE_GAIN);
  return average;
}


uint16_t adc_to_phase_increment(uint16_t value)
{
  // TODO: document these constants
  // minimum frequency for ADC value of 0, offset
  // maximum frequency for largest ADC value, scale factor
  return (32/ADC_AVERAGE_GAIN * value) + PHASE_INC_MIN;
}


uint8_t wave_square(uint8_t phase)
{
  if (phase > 127) {
    return 0xfe;
  } else {
    return 0x01;
  }
}


uint8_t wave_triangle(uint8_t phase)
{
  if (phase < 128) {
    return 2*phase;
  } else {
    return 256 - phase;
  }
}


uint8_t wave_sawtooth(uint8_t phase)
{
  return phase;
}


const uint8_t sine_table[256] = {
  128,131,134,137,140,143,146,149,152,155,158,162,165,167,170,173,
  176,179,182,185,188,190,193,196,198,201,203,206,208,211,213,215,
  218,220,222,224,226,228,230,232,234,235,237,238,240,241,243,244,
  245,246,248,249,250,250,251,252,253,253,254,254,254,255,255,255,
  255,255,255,255,254,254,254,253,253,252,251,250,250,249,248,246,
  245,244,243,241,240,238,237,235,234,232,230,228,226,224,222,220,
  218,215,213,211,208,206,203,201,198,196,193,190,188,185,182,179,
  176,173,170,167,165,162,158,155,152,149,146,143,140,137,134,131,
  128,124,121,118,115,112,109,106,103,100, 97, 93, 90, 88, 85, 82,
   79, 76, 73, 70, 67, 65, 62, 59, 57, 54, 52, 49, 47, 44, 42, 40,
   37, 35, 33, 31, 29, 27, 25, 23, 21, 20, 18, 17, 15, 14, 12, 11,
   10,  9,  7,  6,  5,  5,  4,  3,  2,  2,  1,  1,  1,  0,  0,  0,
    0,  0,  0,  0,  1,  1,  1,  2,  2,  3,  4,  5,  5,  6, 7,  9,
   10, 11, 12, 14, 15, 17, 18, 20, 21, 23, 25, 27, 29, 31, 33, 35,
   37, 40, 42, 44, 47, 49, 52, 54, 57, 59, 62, 65, 67, 70, 73, 76,
   79, 82, 85, 88, 90, 93, 97,100,103,106,109,112,115,118,121,124
};







uint8_t wave_sine(uint8_t phase)
{
  return sine_table[phase];
}




void debug_value(uint16_t value)
{
  volatile char i = 0;

  cli();
  PORTB |= DEBUG_PIN;
  PORTB &= ~DEBUG_PIN;
  while (i < 16) {
    if (value & 0x01) {
      PORTB |= DEBUG_PIN;
      _NOP();
    } else {
      PORTB &= ~DEBUG_PIN;
    }
    i++;
    value >>= 1;
  }
  PORTB |= DEBUG_PIN;
  PORTB &= ~DEBUG_PIN;
  sei();
}

/*
ISR(ADC_vect)
{

}
*/

ISR(TIMER0_COMPA_vect) 
{
  static uint16_t timer_count = 0;

  uint16_t adc, avg;
  uint16_t x;


  // timing-sensitive things first
  // output value
  x = 0xff & (dco_phase >> 8);
  OCR1A = wave_sawtooth(x);


  // then everything else
  timer_count++;


  // ADC
  if ((timer_count + ADC_TICK_OFFSET) % ADC_PERIOD == 0) {
    adc = analogRead(A2);
    average = adc_average(adc);

    // always update phase increment ASAP
    if (average >= MAX_ACTIVE_INPUT) {
      // silent if over-range
      phase_increment = 0;
    }
    else {
      phase_increment = adc_to_phase_increment(average);
    }

    if (alternate_tone
        && ((timer_count % ALTERNATE_PERIOD) < (ALTERNATE_PERIOD / 4))) {
      // alternate between reference and live values
      phase_increment = adc_to_phase_increment(zero_reference);
      PORTB |= DEBUG_PIN;
    }
    else {
      PORTB &= ~DEBUG_PIN;
    }
  } // ADC


  // DCO update
  // (after possible ADC for quicker response)
  dco_phase += phase_increment;


  // Input handling
  if ((timer_count + INPUT_TICK_OFFSET) % INPUT_PERIOD == 0) {
    SM_input();
  }


} // TIMER0_COMPA_vect



void setup()
{
  pin_setup();
  timer_setup();
  adc_setup();
}


void loop()
{
  // all action is ticked by timer0 overflow
}



// vim: sts=2 sw=2
