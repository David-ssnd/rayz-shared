#ifndef BLE_WEAPON_H
#define BLE_WEAPON_H

#include <Arduino.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

class BLEWeapon
{
  private:
    BLEServer* pServer;
    BLECharacteristic* pMessageCharacteristic;
    bool deviceConnected;
    bool oldDeviceConnected;

    class ServerCallbacks : public BLEServerCallbacks
    {
        BLEWeapon* parent;

      public:
        ServerCallbacks(BLEWeapon* p) : parent(p) {}

        void onConnect(BLEServer* pServer)
        {
            parent->deviceConnected = true;
        }

        void onDisconnect(BLEServer* pServer)
        {
            parent->deviceConnected = false;
        }
    };

  public:
    BLEWeapon();
    void begin();
    void sendMessage(uint16_t message);
    bool isConnected();
    void handleConnection();
};

#endif
