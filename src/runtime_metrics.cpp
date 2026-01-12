#include "runtime_metrics.h"
#include <esp_system.h>
#include <esp_timer.h>
#include "game_state.h"


uint32_t system_uptime_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

uint32_t system_free_heap(void)
{
    return esp_get_free_heap_size();
}

int metric_player_id(void)
{
    return (int)game_state_get_player_id();
}

int metric_device_id(void)
{
    const DeviceConfig* cfg = game_state_get_config();
    return cfg ? (int)cfg->device_id : -1;
}

int metric_ammo(void)
{
    return game_state_get_ammo();
}

uint32_t metric_last_rx_ms_ago(void)
{
    return game_state_last_rx_ms_ago();
}

uint32_t metric_rx_count(void)
{
    return game_state_rx_count();
}

uint32_t metric_tx_count(void)
{
    return game_state_tx_count();
}

// Target-specific metrics (weak implementations)
int __attribute__((weak)) metric_hit_count(void)
{
    return 0;
}

uint32_t __attribute__((weak)) metric_last_hit_ms_ago(void)
{
    return 0;
}
