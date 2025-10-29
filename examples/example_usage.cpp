// Example: Using shared headers in target/weapon
#include <Arduino.h>
#include "hash.h"

void setup() {
    Serial.begin(115200);
    
    Serial.println("RayZ Device Starting...");
    Serial.printf("Protocol Version: %s\n", RAYZ_PROTOCOL_VERSION);
    delay(1000);
}

void loop() {
    uint16_t testData = 0x0A5B; // Example 12-bit data
    uint16_t message = createMessage16bit(testData);

    Serial.print("Original Data: 0x");
    Serial.println(testData, HEX);
    Serial.print("16-bit Message with Hash: 0x");
    Serial.println(message, HEX);

    uint16_t extractedData = 0;
    bool isValid = validateMessage16bit(message, &extractedData);
    Serial.print("Is Message Valid? ");
    Serial.println(isValid ? "Yes" : "No");
    if (isValid) {
        Serial.print("Extracted Data: 0x");
        Serial.println(extractedData, HEX);
    }
    
    delay(1000);
}
