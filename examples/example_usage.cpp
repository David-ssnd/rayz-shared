// Example: Using shared headers in target/weapon
#include <Arduino.h>
#include "rayz_common.h"

void setup() {
    Serial.begin(115200);
    
    Serial.println("RayZ Device Starting...");
    Serial.printf("Protocol Version: %s\n", RAYZ_PROTOCOL_VERSION);
    
    #ifdef TARGET_DEVICE
        Serial.println("Device Type: TARGET");
    #elif defined(WEAPON_DEVICE)
        Serial.println("Device Type: WEAPON");
    #endif
}

void loop() {
    // Create a message
    RayZMessage msg;
    
    #ifdef TARGET_DEVICE
        msg.deviceType = DEVICE_TARGET;
    #elif defined(WEAPON_DEVICE)
        msg.deviceType = DEVICE_WEAPON;
    #endif
    
    msg.messageType = MSG_HEARTBEAT;
    msg.payloadLength = 0;
    
    // Send message...
    
    delay(1000);
}
