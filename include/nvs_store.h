#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    bool nvs_store_read_str(const char* ns, const char* key, char* out, size_t max_len);
    bool nvs_store_write_str(const char* ns, const char* key, const char* value);
    bool nvs_store_erase_namespace(const char* ns);
    bool nvs_store_read_u8(const char* ns, const char* key, uint8_t* out);
    bool nvs_store_write_u8(const char* ns, const char* key, uint8_t value);
    bool nvs_store_read_u32(const char* ns, const char* key, uint32_t* out);
    bool nvs_store_write_u32(const char* ns, const char* key, uint32_t value);

#ifdef __cplusplus
}
#endif
