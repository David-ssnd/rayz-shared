#include "wifi_internal.h"

EventGroupHandle_t g_wifi_events = NULL;
wifi_boot_mode_t g_wifi_boot_mode = WIFI_BOOT_PROVISIONING;
char g_wifi_ip[16] = {0};
httpd_handle_t g_httpd = NULL;
char g_device_name[32] = {0};
char g_role[12] = {0};
uint8_t g_wifi_channel = 1;
char g_peer_list[256] = {0};
