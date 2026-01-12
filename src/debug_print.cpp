#include "debug_print.h"
#include "esp_log.h"

static const char* TAG = "DebugPrint";

void debug_print_nvs_contents(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        nvs_flash_erase();
        nvs_flash_init();
    }

    nvs_handle_t handle;
    err = nvs_open(NVS_NS_WIFI, NVS_READONLY, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGI(TAG, "Error opening NVS namespace '%s': %s\n", NVS_NS_WIFI, esp_err_to_name(err));
        return;
    }

    nvs_close(handle);
}
