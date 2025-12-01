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

#ifdef __cplusplus
}
#endif
