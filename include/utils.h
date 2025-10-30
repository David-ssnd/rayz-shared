#ifndef SHARED_UTILS_H
#define SHARED_UTILS_H

#include <Arduino.h>

/**
 * @brief Convert a value to binary string representation with spaces every 4 bits
 * @param value The value to convert
 * @param bits Number of bits to display
 * @return Binary string with spaces every 4 bits for readability
 */
String toBinaryString(uint16_t value, int bits = 16);

#endif
