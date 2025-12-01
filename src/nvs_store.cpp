#include "nvs_store.h"
#include <esp_log.h>
#include <nvs.h>
#include <nvs_flash.h>


static const char* TAG = "NVSStore";

bool nvs_store_read_str(const char* ns, const char* key, char* out, size_t max_len)
{
    if (!ns || !key || !out)
        return false;
    nvs_handle_t handle;
    if (nvs_open(ns, NVS_READONLY, &handle) != ESP_OK)
        return false;
    size_t required = max_len;
    esp_err_t err = nvs_get_str(handle, key, out, &required);
    nvs_close(handle);
    if (err == ESP_OK)
        return true;
    return false;
}

bool nvs_store_write_str(const char* ns, const char* key, const char* value)
{
    if (!ns || !key || !value)
        return false;
    nvs_handle_t handle;
    if (nvs_open(ns, NVS_READWRITE, &handle) != ESP_OK)
        return false;
    esp_err_t err = nvs_set_str(handle, key, value);
    if (err == ESP_OK)
        err = nvs_commit(handle);
    nvs_close(handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed write %s:%s err=%d", ns, key, err);
        return false;
    }
    return true;
}

bool nvs_store_erase_namespace(const char* ns)
{
    if (!ns)
        return false;
    nvs_handle_t handle;
    if (nvs_open(ns, NVS_READWRITE, &handle) != ESP_OK)
        return false;
    esp_err_t err = nvs_erase_all(handle);
    if (err == ESP_OK)
        err = nvs_commit(handle);
    nvs_close(handle);
    return err == ESP_OK;
}
