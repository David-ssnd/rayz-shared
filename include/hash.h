#ifndef HASH_H
#define HASH_H

#include <stdint.h>
#include "protocol_config.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

/**
 * @brief Calculate 8-bit hash for message integrity
 * 
 * @param data The 8-bit data value to hash
 * @return uint8_t The calculated 8-bit hash value
 */
inline uint8_t calculateHash8bit(uint16_t data) {
    uint8_t hash = (((data & 0xFF) ^ HASH_XOR_SEED) + HASH_OFFSET) & 0xFF;
    return hash;
}

/**
 * @brief Validate a 16-bit message by checking its hash
 * 
 * Message format: [8-bit data][8-bit hash]
 * 
 * @param message The 16-bit message to validate
 * @param outData Optional pointer to store the extracted data if valid
 * @return true if the hash is valid, false otherwise
 */
inline bool validateMessage16bit(uint16_t message, uint16_t* outData = nullptr) {
    uint16_t data = (message >> MESSAGE_HASH_BITS) & 0xFF;
    uint8_t receivedHash = message & 0xFF;
    
    uint8_t expectedHash = calculateHash8bit(data);
    
    if (receivedHash == expectedHash) {
        if (outData != nullptr) {
            *outData = data;
        }
        return true;
    }
    
    return false;
}

/**
 * @brief Create a 16-bit message with data and hash
 * 
 * Message format: [8-bit data][8-bit hash]
 * 
 * @param data The data value to encode (only lower 8 bits used)
 * @return uint16_t The complete 16-bit message with hash
 */
inline uint16_t createMessage16bit(uint16_t data) {
    data &= 0xFF;
    
    uint8_t hash = calculateHash8bit(data);
    
    uint16_t message = (data << MESSAGE_HASH_BITS) | (hash & 0xFF);
    
    return message;
}

#endif // HASH_H
