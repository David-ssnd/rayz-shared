idf_component_register(
    SRCS 
        # "src/ble_target.cpp"
        # "src/ble_weapon.cpp"
        "src/utils.cpp"
        "src/nvs_store.cpp"
        "src/wifi_manager.cpp"
        "src/wifi_state.cpp"
        "src/wifi_core.cpp"
        "src/wifi_http.cpp"
        "src/http_api.cpp"
        "src/ws_server.cpp"
        "src/game_state.cpp"
        "src/espnow_comm.cpp"
        "src/display_init.cpp"
        "src/display_manager.cpp"
        "src/runtime_metrics.cpp"
        "src/debug_print.cpp"
        "src/gpio_init.cpp"
    INCLUDE_DIRS 
        "include"
    REQUIRES
        nvs_flash
        esp_wifi
        esp_coex
        esp_netif
        esp_event
        esp_http_server
        json
        driver
        esp_timer
        esp_lcd
    PRIV_REQUIRES
        freertos
)

# Find LVGL dynamically based on the project being built
# PlatformIO puts libraries in .pio/libdeps/<env_name>/lvgl
set(LVGL_SEARCH_PATHS
    "${PROJECT_DIR}/.pio/libdeps/target/lvgl"
    "${PROJECT_DIR}/.pio/libdeps/weapon/lvgl"
)

foreach(LVGL_PATH ${LVGL_SEARCH_PATHS})
    if(EXISTS "${LVGL_PATH}")
        message(STATUS "Found LVGL at: ${LVGL_PATH}")
        target_include_directories(${COMPONENT_LIB} PUBLIC "${LVGL_PATH}")
        # Also include device-specific config for lv_conf.h
        target_include_directories(${COMPONENT_LIB} PUBLIC "${PROJECT_DIR}/../${CMAKE_PROJECT_NAME}/include")
        break()
    endif()
endforeach()
