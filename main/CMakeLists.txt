idf_component_register(
    SRCS "main.cpp" "app.cpp" "ui.cpp" "art.cpp" "safari.cpp" "safari_txt.cpp"
    INCLUDE_DIRS "./"
    PRIV_REQUIRES driver esp_timer esp_wifi esp_event esp_netif nvs_flash
                  esp_http_server esp_driver_usb_serial_jtag esp_driver_tsens
                  esp_http_client esp-tls mbedtls
                  board_power_bsp epaper_driver_bsp)
