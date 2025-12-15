#pragma once

#include <esp_now.h>
#include <stdbool.h>
#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#ifdef __cplusplus
extern "C" {
#endif

// Compact player-to-player message carried over ESP-NOW.
typedef enum : uint8_t
{
    ESPNOW_MSG_SHOT = 0,
    ESPNOW_MSG_HIT_EVENT,
    ESPNOW_MSG_HEARTBEAT,
} EspnowMsgType;

typedef struct __attribute__((packed))
{
    EspnowMsgType type;
    uint8_t version;
    uint8_t player_id;
    uint8_t device_id;
    uint8_t team_id;
    uint8_t reserved;
    uint32_t color_rgb;
    uint32_t timestamp_ms;
    uint32_t data;
} PlayerMessage;

typedef struct
{
    PlayerMessage msg;
    uint8_t src_mac[ESP_NOW_ETH_ALEN];
} EspnowMessageEnvelope;

typedef struct
{
    uint8_t channel;   // 0 = keep current Wi-Fi channel, otherwise lock to specific channel
    bool prefer_wifi;  // Set coexistence preference towards Wi-Fi if true
    bool set_pmk;      // Configure PMK to a non-zero key (recommended)
} EspnowCommConfig;

// Initialise ESP-NOW (idempotent).
esp_err_t espnow_comm_init(const EspnowCommConfig* config);

// Force Wi-Fi channel used by ESP-NOW (must match AP channel when STA is up).
esp_err_t espnow_comm_set_channel(uint8_t channel);

// Peer management helpers.
esp_err_t espnow_comm_add_peer(const uint8_t mac[ESP_NOW_ETH_ALEN]);
void espnow_comm_clear_peers(void);
uint8_t espnow_comm_peer_count(void);
esp_err_t espnow_comm_load_peers_from_csv(const char* csv_list); // "aa:bb:cc...,11:22:33..."

// Send helpers.
bool espnow_comm_send(const uint8_t mac[ESP_NOW_ETH_ALEN], const PlayerMessage* msg);
bool espnow_comm_broadcast(const PlayerMessage* msg); // Broadcast to all peers

// Receive queue helpers.
QueueHandle_t espnow_comm_queue(void);
bool espnow_comm_receive(EspnowMessageEnvelope* out, TickType_t ticks_to_wait);

uint8_t espnow_comm_hash_id(const char* id);

#ifdef __cplusplus
}
#endif
