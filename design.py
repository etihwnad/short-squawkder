#!/usr/bin/env python3


# Interactive-ish script to play with and compute constants for the
# ShortSquawker.


try:
    # clear the IPython environment so we start clean each time
    from IPython import get_ipython
    get_ipython().magic('reset -sf')
except:
    pass


variables = locals().copy()


SYSTEM_CLK = 8e6
ADC_AVERAGE_GAIN = 32


desired_fs = 1e3

# find a prescaler value and max value to yield
# the desired sample rate
timer0divs = (1, 8, 64, 256, 1024)
for TIMER0_DIV in timer0divs:
    TIMER0_CLK = SYSTEM_CLK / TIMER0_DIV
    TIMER0_MAX = int(round(TIMER0_CLK / desired_fs - 1))
    TIMER0_INT_RATE = TIMER0_CLK / (TIMER0_MAX + 1)

    if TIMER0_MAX <= 255:
        break


PLL_CLK = SYSTEM_CLK * 8
TIMER1_DIV = 1
TIMER1_CLK = PLL_CLK / TIMER1_DIV

OCR1C = 255

PWM_F = TIMER1_CLK / (OCR1C + 1)



xnames = set(locals().copy().keys())
vnames = set(variables.keys())


print()
print('Input variables:')
print('----------------')
for n in sorted(xnames - vnames):
    if n.startswith('desired'):
        print(f'{n}: {locals()[n]}')
print()
for n in sorted(xnames - vnames):
    if n[0].isupper():
        print(f'{n}: {locals()[n]}')





def f_dco(inc, f_clk=TIMER0_INT_RATE, dco_bits=16):
    return f_clk * inc / 2**dco_bits

def p_dco(f, f_clk=TIMER0_INT_RATE, dco_bits=16):
    return f * 2**dco_bits / f_clk

print()

print('Timer0 interrupt rate:')
print(f'fs = {TIMER0_INT_RATE:6.1f}')

print('\nTimer1 PWM frequency:')
print(f'f PWM = {PWM_F}')

print('\nDCO frequencies:')
fout_max = f_dco(ADC_AVERAGE_GAIN * 1023)
fout_min = f_dco(1023)
print(f'f max: {fout_max:6.1f}')
print(f'f min: {fout_min:6.1f}')

print()
print(p_dco(1e3))



