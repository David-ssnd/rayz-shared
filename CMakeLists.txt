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
)
