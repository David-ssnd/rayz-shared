#include "ble_target.h"
#include "ble_config.h"
#include "protocol_config.h"

static BLETarget* bleTargetInstance = nullptr;

BLETarget::BLETarget()
    : pClient(nullptr), pRemoteCharacteristic(nullptr), connected(false), doConnect(false),
      targetDevice(nullptr), lastReceivedMessage(0), hasNewMessage(false)
{
    bleTargetInstance = this;
}

void BLETarget::notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData,
                               size_t length, bool isNotify)
{
    if (bleTargetInstance && length == 2)
    {
        uint16_t message = (pData[0] << 8) | pData[1];
        bleTargetInstance->lastReceivedMessage = message;
        bleTargetInstance->hasNewMessage = true;
    }
}

void BLETarget::begin()
{
    Serial.println("Initializing BLE Target...");
    BLEDevice::init(BLE_TARGET_NAME);
    scan();
}

void BLETarget::scan()
{
    Serial.println("Scanning for BLE Weapon...");

    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks(this));
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(BLE_SCAN_TIME, false);
}

bool BLETarget::connectToServer()
{
    if (!targetDevice)
    {
        return false;
    }

    Serial.print("Connecting to ");
    Serial.println(targetDevice->getAddress().toString().c_str());

    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new ClientCallbacks(this));

    if (!pClient->connect(targetDevice))
    {
        Serial.println("Failed to connect to server");
        return false;
    }
    Serial.println("Connected to server");

    BLERemoteService* pRemoteService = pClient->getService(BLE_SERVICE_UUID);
    if (pRemoteService == nullptr)
    {
        Serial.print("Failed to find service UUID: ");
        Serial.println(BLE_SERVICE_UUID);
        pClient->disconnect();
        return false;
    }
    Serial.println("Found service");

    pRemoteCharacteristic = pRemoteService->getCharacteristic(BLE_MESSAGE_CHAR_UUID);
    if (pRemoteCharacteristic == nullptr)
    {
        Serial.print("Failed to find characteristic UUID: ");
        Serial.println(BLE_MESSAGE_CHAR_UUID);
        pClient->disconnect();
        return false;
    }
    Serial.println("Found characteristic");

    if (pRemoteCharacteristic->canNotify())
    {
        pRemoteCharacteristic->registerForNotify(notifyCallback);
        Serial.println("Registered for notifications");
    }

    connected = true;
    Serial.println("BLE connection established!");
    return true;
}

void BLETarget::update()
{
    if (doConnect)
    {
        if (connectToServer())
        {
            Serial.println("Successfully connected to weapon");
        }
        else
        {
            Serial.println("Failed to connect, will retry scanning...");
            delay(BLE_RETRY_DELAY_MS);
            scan();
        }
        doConnect = false;
    }

    if (!connected && pClient)
    {
        Serial.println("Lost connection. Scanning again...");
        scan();
    }
}

bool BLETarget::isConnected()
{
    return connected;
}

bool BLETarget::hasMessage()
{
    return hasNewMessage;
}

uint16_t BLETarget::getMessage()
{
    hasNewMessage = false;
    return lastReceivedMessage;
}
