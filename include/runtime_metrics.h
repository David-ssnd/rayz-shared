#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    uint32_t system_uptime_ms(void);
    uint32_t system_free_heap(void);
    int metric_player_id(void);
    int metric_device_id(void);
    int metric_ammo(void);
    uint32_t metric_last_rx_ms_ago(void);
    uint32_t metric_rx_count(void);
    uint32_t metric_tx_count(void);

    // Target-specific metrics
    int metric_hit_count(void);
    uint32_t metric_last_hit_ms_ago(void);

#ifdef __cplusplus
}
#endif
