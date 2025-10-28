#ifndef HASH_H
#define HASH_H

#include <stdint.h>

#ifdef ARDUINO
#include <Arduino.h>
#endif

#define RAYZ_PROTOCOL_VERSION "1.0.0"

#define MAX_MESSAGE_SIZE 256
#define COMMUNICATION_TIMEOUT 5000

enum DeviceType {
    DEVICE_TARGET = 0x01,
    DEVICE_WEAPON = 0x02
};

enum MessageType {
    MSG_HEARTBEAT = 0x01,
    MSG_STATUS = 0x02,
    MSG_COMMAND = 0x03,
    MSG_DATA = 0x04
};

struct RayZMessage {
    uint8_t deviceType;
    uint8_t messageType;
    uint16_t payloadLength;
    uint8_t payload[MAX_MESSAGE_SIZE];
    uint16_t checksum;
};

// Photodiode protocol: 12 bits data + 4 bits hash
#define PHOTODIODE_BUFFER_SIZE 16
#define PHOTODIODE_DATA_BITS 12
#define PHOTODIODE_HASH_BITS 4

// Calculate 4-bit hash for 12-bit data (prevents 0x0000 and 0xFFFF)
inline uint8_t calculateHash4bit(uint16_t data) {
    data &= 0x0FFF;
    
    uint8_t nibble0 = data & 0x0F;
    uint8_t nibble1 = (data >> 4) & 0x0F;
    uint8_t nibble2 = (data >> 8) & 0x0F;
    
    // Simple XOR-based hash with a constant (randomly chosen)
    uint8_t hash = (nibble0 ^ nibble1 ^ nibble2 ^ 0x09 + 1) & 0x0F;
    
    return hash;
}

// Validate 16-bit message and extract data
inline bool validateMessage16bit(uint16_t message, uint16_t* outData) {
    uint16_t data = (message >> 4) & 0x0FFF;
    uint8_t receivedHash = message & 0x0F;
    uint8_t expectedHash = calculateHash4bit(data);
    
    if (receivedHash == expectedHash) {
        if (outData != nullptr) {
            *outData = data;
        }
        return true;
    }
    
    return false;
}

// Create 16-bit message with hash
inline uint16_t createMessage16bit(uint16_t data) {
    data &= 0x0FFF;
    uint8_t hash = calculateHash4bit(data);
    uint16_t message = (data << 4) | (hash & 0x0F);
    
    return message;
}

#endif // HASH_H
