#!/usr/bin/env python3





SYSTEM_CLK = 8e6


ADC_AVERAGE_GAIN = 32

TIMER0_DIV = 8
TIMER0_CLK = SYSTEM_CLK / TIMER0_DIV

TIMER0_MAX = 199
TIMER0_INT_RATE = TIMER0_CLK / (TIMER0_MAX + 1)


PLL_CLK = SYSTEM_CLK * 8
TIMER1_DIV = 1
TIMER1_CLK = PLL_CLK / TIMER1_DIV

OCR1C = 255

PWM_F = TIMER1_CLK / (OCR1C + 1)



def f_dco(inc, f_clk=TIMER0_INT_RATE, dco_bits=16):
    return f_clk * inc / 2**dco_bits

def p_dco(f, f_clk=TIMER0_INT_RATE, dco_bits=16):
    return f * 2**dco_bits / f_clk


print('Timer0 interrupt rate:')
print(f'fs = {TIMER0_INT_RATE:6.1f}')

print('\nTimer1 PWM frequency:')
print(f'f PWM = {PWM_F}')

print('\nDCO frequencies:')
fout_max = f_dco(ADC_AVERAGE_GAIN * 1023)
fout_min = f_dco(1023)
print(f'f max: {fout_max:6.1f}')
print(f'f min: {fout_min:6.1f}')



