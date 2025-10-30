#include "ble_weapon.h"
#include "ble_config.h"

BLEWeapon::BLEWeapon() : pServer(nullptr), pMessageCharacteristic(nullptr), 
                          deviceConnected(false), oldDeviceConnected(false) {}

void BLEWeapon::begin() {
    Serial.println("Initializing BLE Weapon...");
    
    // Create the BLE Device
    BLEDevice::init(BLE_WEAPON_NAME);
    
    // Create the BLE Server
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks(this));
    
    // Create the BLE Service
    BLEService* pService = pServer->createService(BLE_SERVICE_UUID);
    
    // Create a BLE Characteristic for sending messages
    pMessageCharacteristic = pService->createCharacteristic(
        BLE_MESSAGE_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    
    // Add descriptor for notifications
    pMessageCharacteristic->addDescriptor(new BLE2902());
    
    // Start the service
    pService->start();
    
    // Start advertising
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    
    Serial.println("BLE Weapon ready. Waiting for a target to connect...");
}

void BLEWeapon::sendMessage(uint16_t message) {
    if (deviceConnected) {
        uint8_t data[2];
        data[0] = (message >> 8) & 0xFF;  // MSB
        data[1] = message & 0xFF;          // LSB
        
        pMessageCharacteristic->setValue(data, 2);
        pMessageCharacteristic->notify();
    }
}

bool BLEWeapon::isConnected() {
    return deviceConnected;
}

void BLEWeapon::handleConnection() {
    // Handle disconnection
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // Give the bluetooth stack time to get ready
        pServer->startAdvertising();
        Serial.println("BLE disconnected. Started advertising again...");
        oldDeviceConnected = deviceConnected;
    }
    
    // Handle connection
    if (deviceConnected && !oldDeviceConnected) {
        Serial.println("BLE Target connected!");
        oldDeviceConnected = deviceConnected;
    }
}
