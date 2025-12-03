#ifndef BLE_WEAPON_H
#define BLE_WEAPON_H

#include <freertos/FreeRTOS.h>

#include <freertos/event_groups.h>
#include <cstdint>
#include <host/ble_gap.h>
#include <host/ble_gatt.h>
#include <host/ble_hs.h>
#include <host/ble_uuid.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <nvs_flash.h>

class BLEWeapon
{
  private:
    uint16_t conn_handle;
    bool deviceConnected;
    bool oldDeviceConnected;
    uint16_t message_char_handle;

    EventGroupHandle_t event_group;

    static BLEWeapon* instance;

    static int gap_event_handler(struct ble_gap_event* event, void* arg);
    static int gatt_svr_chr_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt,
                                   void* arg);

    void start_advertising();

    ble_uuid128_t service_uuid;
    ble_uuid128_t message_uuid;
    struct ble_gatt_chr_def chr_defs[2];
    struct ble_gatt_svc_def svc_defs[2];

  public:
    BLEWeapon();
    void begin();
    void sendMessage(uint16_t message);
    bool isConnected();
    void handleConnection();
};

#endif
