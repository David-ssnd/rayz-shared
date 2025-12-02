#ifndef BLE_TARGET_H
#define BLE_TARGET_H

#include <freertos/FreeRTOS.h>

#include "ble_config.h"
#include <cstdint>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <host/ble_gap.h>
#include <host/ble_gatt.h>
#include <host/ble_hs.h>
#include <host/ble_uuid.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <nvs_flash.h>
#include <stdint.h>

class BLETarget
{
  public:
    BLETarget();
    void begin();
    void update();
    bool isConnected();
    bool hasMessage();
    uint16_t getMessage();
    bool fetchMessage(uint16_t* message, TickType_t ticksToWait = 0);

  private:
    uint16_t conn_handle;
    uint16_t message_char_handle;
    bool connected;
    uint16_t lastReceivedMessage;
    bool hasNewMessage;

    EventGroupHandle_t event_group;
    QueueHandle_t message_queue;

    ble_uuid128_t service_uuid;
    ble_uuid128_t message_uuid;

    static BLETarget* instance;

    static int gap_event_handler(struct ble_gap_event* event, void* arg);
    static int on_service_discovery(uint16_t conn_handle, const struct ble_gatt_error* error,
                                    const struct ble_gatt_svc* service, void* arg);
    static int on_characteristic_discovery(uint16_t conn_handle, const struct ble_gatt_error* error,
                                           const struct ble_gatt_chr* chr, void* arg);

    void start_scan();
    void connect_to_device(const ble_addr_t* addr);
    int enable_notifications(uint16_t conn_handle) const;
};

#endif
