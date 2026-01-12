#include "espnow_comm.h"
#include <esp_coexist.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <ctype.h>
#include <string.h>
#include "hash.h"

static const uint8_t ESPNOW_PMK[ESP_NOW_KEY_LEN] = {'r', 'a', 'y', 'z', '-', 'e', 's', 'p',
                                                    'n', 'o', 'w', '-', 'p', 'm', 'k', '!'};

static const char* TAG = "EspNowComm";
static_assert(sizeof(EspnowMsgType) == 1, "EspnowMsgType must stay 1 byte");
static bool s_initialised = false;
static uint8_t s_channel = 0;
static QueueHandle_t s_rx_queue = NULL;
static SemaphoreHandle_t s_send_mutex = NULL;
static uint8_t s_peer_count = 0;

static void log_mac(const uint8_t mac[ESP_NOW_ETH_ALEN], const char* prefix)
{
    ESP_LOGI(TAG, "%s %02X:%02X:%02X:%02X:%02X:%02X", prefix ? prefix : "MAC", mac[0], mac[1], mac[2], mac[3], mac[4],
             mac[5]);
}

uint8_t espnow_comm_hash_id(const char* id)
{
    // Fold the ID with the same 8-bit hash used for laser messages.
    uint8_t hash = 0;
    if (id)
    {
        while (*id)
        {
            hash = calculateHash8bit(hash ^ (uint8_t)(*id));
            id++;
        }
    }
    return hash;
}

static bool parse_mac_str(const char* str, uint8_t mac[ESP_NOW_ETH_ALEN])
{
    int values[ESP_NOW_ETH_ALEN] = {0};
    if (sscanf(str, "%x:%x:%x:%x:%x:%x", &values[0], &values[1], &values[2], &values[3], &values[4], &values[5]) == 6 ||
        sscanf(str, "%x-%x-%x-%x-%x-%x", &values[0], &values[1], &values[2], &values[3], &values[4], &values[5]) == 6)
    {
        for (int i = 0; i < ESP_NOW_ETH_ALEN; i++)
        {
            mac[i] = (uint8_t)values[i];
        }
        return true;
    }
    return false;
}

static void recv_cb(const esp_now_recv_info_t* info, const uint8_t* data, int len)
{
    if (!info || !data || len != sizeof(PlayerMessage))
    {
        ESP_LOGW(TAG, "RX invalid len=%d", len);
        return;
    }

    if (!s_rx_queue)
        return;

    EspnowMessageEnvelope env = {};
    memcpy(env.src_mac, info->src_addr, ESP_NOW_ETH_ALEN);
    memcpy(&env.msg, data, sizeof(PlayerMessage));

    BaseType_t hp_task_woken = pdFALSE;
    xQueueSendFromISR(s_rx_queue, &env, &hp_task_woken);
    if (hp_task_woken == pdTRUE)
    {
        portYIELD_FROM_ISR();
    }
}

static void send_cb(const esp_now_send_info_t* info, esp_now_send_status_t status)
{
    (void)info;
    if (status != ESP_NOW_SEND_SUCCESS)
    {
        ESP_LOGW(TAG, "Send status: %d", status);
    }
}

esp_err_t espnow_comm_init(const EspnowCommConfig* config)
{
    if (s_initialised)
    {
        return ESP_OK;
    }

    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) == ESP_ERR_WIFI_NOT_INIT)
    {
        wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
        esp_wifi_init(&wifi_cfg);
    }

    esp_err_t err = esp_now_init();
    if (err == ESP_ERR_ESPNOW_EXIST)
    {
        err = ESP_OK;
    }
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_now_init failed: %s", esp_err_to_name(err));
        return err;
    }

    if (!s_rx_queue)
    {
        s_rx_queue = xQueueCreate(16, sizeof(EspnowMessageEnvelope));
    }
    if (!s_send_mutex)
    {
        s_send_mutex = xSemaphoreCreateMutex();
    }

    esp_now_register_recv_cb(recv_cb);
    esp_now_register_send_cb(send_cb);
    s_peer_count = 0;

    if (config)
    {
        if (config->set_pmk)
        {
            esp_now_set_pmk(ESPNOW_PMK);
        }
        if (config->channel > 0)
        {
            espnow_comm_set_channel(config->channel);
        }
        if (config->prefer_wifi)
        {
            esp_coex_preference_set(ESP_COEX_PREFER_WIFI);
        }
    }

    s_initialised = true;
    ESP_LOGI(TAG, "ESP-NOW ready%s", s_channel ? " with fixed channel" : "");
    return ESP_OK;
}

esp_err_t espnow_comm_set_channel(uint8_t channel)
{
    if (channel == 0)
        return ESP_OK;
    uint8_t current_primary = 0;
    wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
    esp_wifi_get_channel(&current_primary, &second); // keep current secondary
    esp_err_t err = esp_wifi_set_channel(channel, second);
    if (err == ESP_OK)
    {
        s_channel = channel;
        ESP_LOGI(TAG, "ESP-NOW channel locked to %u", channel);
    }
    else
    {
        ESP_LOGW(TAG, "Failed to set channel %u: %s", channel, esp_err_to_name(err));
    }
    return err;
}

esp_err_t espnow_comm_add_peer(const uint8_t mac[ESP_NOW_ETH_ALEN])
{
    if (!mac)
        return ESP_ERR_INVALID_ARG;

    if (esp_now_is_peer_exist(mac))
    {
        return ESP_OK;
    }

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, ESP_NOW_ETH_ALEN);
    peer.channel = s_channel; // 0 = current channel
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;

    esp_err_t err = esp_now_add_peer(&peer);
    if (err == ESP_OK)
    {
        s_peer_count++;
        log_mac(mac, "Peer added");
    }
    else
    {
        ESP_LOGW(TAG, "Failed to add peer: %s", esp_err_to_name(err));
    }
    return err;
}

void espnow_comm_clear_peers(void)
{
    esp_now_deinit();
    s_initialised = false;
    s_peer_count = 0;
}

uint8_t espnow_comm_peer_count(void)
{
    return s_peer_count;
}

esp_err_t espnow_comm_load_peers_from_csv(const char* csv_list)
{
    if (!csv_list || !*csv_list)
        return ESP_OK;

    char buffer[256] = {0};
    strncpy(buffer, csv_list, sizeof(buffer) - 1);

    char* token = strtok(buffer, ",;");
    uint8_t mac[ESP_NOW_ETH_ALEN];
    uint8_t loaded = 0;
    while (token)
    {
        while (isspace((int)*token))
            token++;
        if (parse_mac_str(token, mac))
        {
            if (espnow_comm_add_peer(mac) == ESP_OK)
            {
                loaded++;
            }
        }
        token = strtok(NULL, ",;");
    }

    ESP_LOGI(TAG, "Loaded %u peers from list", loaded);
    return loaded > 0 ? ESP_OK : ESP_FAIL;
}

bool espnow_comm_send(const uint8_t mac[ESP_NOW_ETH_ALEN], const PlayerMessage* msg)
{
    if (!msg)
        return false;
    if (!s_send_mutex)
        return false;

    bool ok = false;
    if (xSemaphoreTake(s_send_mutex, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        esp_err_t err = esp_now_send(mac, (const uint8_t*)msg, sizeof(PlayerMessage));
        ok = (err == ESP_OK);
        if (!ok)
        {
            ESP_LOGW(TAG, "esp_now_send failed: %s", esp_err_to_name(err));
        }
        xSemaphoreGive(s_send_mutex);
    }
    return ok;
}

bool espnow_comm_broadcast(const PlayerMessage* msg)
{
    static const uint8_t broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    return espnow_comm_send(broadcast_mac, msg);
}

QueueHandle_t espnow_comm_queue(void)
{
    return s_rx_queue;
}

bool espnow_comm_receive(EspnowMessageEnvelope* out, TickType_t ticks_to_wait)
{
    if (!s_rx_queue || !out)
        return false;
    return xQueueReceive(s_rx_queue, out, ticks_to_wait) == pdTRUE;
}
