#ifndef PERIPH_H
#define PERIPH_H

#include <avr/io.h>

// reading the joystick (analog-to-digital converter)

// get the ADC ready to read the analog pins
static inline void ADC_init(void) {
    ADMUX  = (1 << REFS0);       // measure against the 5V supply
    // switch the ADC on, and slow its clock down enough to read accurately
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

// read one analog pin (channel 0..7) and give back a number 0..1023
static inline uint16_t ADC_read(uint8_t ch) {
    ch &= 0x07;                  // only channels 0-7 exist
    ADMUX = (ADMUX & 0xF8) | ch; // pick the channel
    ADCSRA |= (1 << ADSC);       // kick off a reading
    while (ADCSRA & (1 << ADSC)) { }   // wait for it to finish
    uint8_t low  = ADCL;         // read low byte first, then high (order matters)
    uint8_t high = ADCH;
    return ((uint16_t)high << 8) | low;
}

// rescale a number from one range into another (e.g. joystick 0..1023 -> 4..27),
// and clamp it so it never goes outside the target range
static inline long map_value(long in_min, long in_max,
                             long out_min, long out_max,
                             long x) {
    if (x < in_min) x = in_min;
    if (x > in_max) x = in_max;
    return out_min + (x - in_min) * (out_max - out_min) / (in_max - in_min);
}

// return 'x' with bit number 'k' forced to 1 or 0 (whichever 'b' is)
static inline uint8_t SetBit(uint8_t x, uint8_t k, uint8_t b) {
    return b ? (x | (1 << k)) : (x & ~(1 << k));
}

#endif /* PERIPH_H */