#ifndef DEBUG_PRINT_H
#define DEBUG_PRINT_H

#include "nvs_flash.h"
#include "nvs_store.h"
#include "wifi_internal.h"

/**
 * @brief Debug function to print all NVS contents to log
 */
void debug_print_nvs_contents(void);

#endif // DEBUG_PRINT_H
