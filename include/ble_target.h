#ifndef BLE_TARGET_H
#define BLE_TARGET_H

#include "ble_config.h"
#include <Arduino.h>
#include <BLEClient.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEUtils.h>

class BLETarget
{
  private:
    BLEClient* pClient;
    BLERemoteCharacteristic* pRemoteCharacteristic;
    bool connected;
    bool doConnect;
    BLEAdvertisedDevice* targetDevice;
    uint16_t lastReceivedMessage;
    bool hasNewMessage;

    class ClientCallbacks : public BLEClientCallbacks
    {
        BLETarget* parent;

      public:
        ClientCallbacks(BLETarget* p) : parent(p) {}

        void onConnect(BLEClient* pClient)
        {
            parent->connected = true;
        }

        void onDisconnect(BLEClient* pClient)
        {
            parent->connected = false;
        }
    };

    class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
    {
        BLETarget* parent;

      public:
        AdvertisedDeviceCallbacks(BLETarget* p) : parent(p) {}

        void onResult(BLEAdvertisedDevice advertisedDevice)
        {
            if (advertisedDevice.haveName() && advertisedDevice.getName() == BLE_WEAPON_NAME)
            {
                BLEDevice::getScan()->stop();
                parent->targetDevice = new BLEAdvertisedDevice(advertisedDevice);
                parent->doConnect = true;
            }
        }
    };

    static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData,
                               size_t length, bool isNotify);

  public:
    BLETarget();
    void begin();
    void scan();
    bool connectToServer();
    void update();
    bool isConnected();
    bool hasMessage();
    uint16_t getMessage();
};

#endif
