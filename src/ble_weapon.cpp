#include "ble_weapon.h"
#include "ble_config.h"
#include "protocol_config.h"

BLEWeapon::BLEWeapon() : pServer(nullptr), pMessageCharacteristic(nullptr), 
                          deviceConnected(false), oldDeviceConnected(false) {}

void BLEWeapon::begin() {
    Serial.println("Initializing BLE Weapon...");
    
    BLEDevice::init(BLE_WEAPON_NAME);
    
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks(this));
    
    BLEService* pService = pServer->createService(BLE_SERVICE_UUID);
    
    pMessageCharacteristic = pService->createCharacteristic(
        BLE_MESSAGE_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    
    pMessageCharacteristic->addDescriptor(new BLE2902());
    
    pService->start();
    
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
        data[0] = (message >> 8) & 0xFF;
        data[1] = message & 0xFF;
        
        pMessageCharacteristic->setValue(data, 2);
        pMessageCharacteristic->notify();
    }
}

bool BLEWeapon::isConnected() {
    return deviceConnected;
}

void BLEWeapon::handleConnection() {
    if (!deviceConnected && oldDeviceConnected) {
        delay(BLE_RECONNECT_DELAY_MS);
        pServer->startAdvertising();
        Serial.println("BLE disconnected. Started advertising again...");
        oldDeviceConnected = deviceConnected;
    }
    
    if (deviceConnected && !oldDeviceConnected) {
        Serial.println("BLE Target connected!");
        oldDeviceConnected = deviceConnected;
    }
}
