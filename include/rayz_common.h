#ifndef RAYZ_COMMON_H
#define RAYZ_COMMON_H

#include <Arduino.h>

// Version information
#define RAYZ_PROTOCOL_VERSION "1.0.0"

// Common constants
#define MAX_MESSAGE_SIZE 256
#define COMMUNICATION_TIMEOUT 5000

// Device types
enum DeviceType {
    DEVICE_TARGET = 0x01,
    DEVICE_WEAPON = 0x02
};

// Message types
enum MessageType {
    MSG_HEARTBEAT = 0x01,
    MSG_STATUS = 0x02,
    MSG_COMMAND = 0x03,
    MSG_DATA = 0x04
};

// Message structure
struct RayZMessage {
    uint8_t deviceType;
    uint8_t messageType;
    uint16_t payloadLength;
    uint8_t payload[MAX_MESSAGE_SIZE];
    uint16_t checksum;
};

#endif // RAYZ_COMMON_H
