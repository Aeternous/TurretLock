/*        Name & E-mail:
Pranae Harirajan, phari012@ucr.edu
 *        Assignment:
Custom Laboratory Project - TurretLock - Demo 2
 */

#ifndef PERIPH_H
#define PERIPH_H

#include <avr/io.h>

// Get the ADC ready so we can read the analog joystick pins
static inline void ADC_init() {
  ADMUX  = (1 << REFS0); // measure against the 5V supply
  ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // ADC on, slow clock
}

// Read one analog pin (channel 0-7) and give back a value 0-1023
static inline unsigned int ADC_read(unsigned char chnl) {
  chnl &= 0x07;                     // only channels 0-7 exist
  ADMUX = (ADMUX & 0xF8) | chnl;    // pick the channel
  ADCSRA |= (1 << ADSC);            // start a reading
  while (ADCSRA & (1 << ADSC)) {}   // wait for it to finish

  unsigned char low  = ADCL;        // read low byte first, then high (order matters)
  unsigned char high = ADCH;
  return ((unsigned int)high << 8) | low;
}

// Rescale a value from one range into another (joystick 0-1023 -> 4-27 etc.)
// and clamp it so it never leaves the target range
static inline long map_value(long fromLow, long fromHigh, long toLow, long toHigh, long value) {
  if (value < fromLow)  { value = fromLow; }
  if (value > fromHigh) { value = fromHigh; }
  return toLow + (value - fromLow) * (toHigh - toLow) / (fromHigh - fromLow);
}

// Return 'x' with bit number 'k' forced to 1 or 0 (whichever 'b' is)
static inline unsigned char SetBit(unsigned char x, unsigned char k, unsigned char b) {
  return (b ? (x | (0x01 << k)) : (x & ~(0x01 << k)));
}

#endif /* PERIPH_H */
