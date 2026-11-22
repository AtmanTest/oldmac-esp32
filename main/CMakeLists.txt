idf_component_register(
    SRCS "main.cpp" "app.cpp" "ui.cpp" "art.cpp" "safari.cpp" "safari_txt.cpp"
    INCLUDE_DIRS "./"
    PRIV_REQUIRES driver esp_timer esp_wifi esp_event esp_netif nvs_flash
                  esp_http_server esp_driver_usb_serial_jtag esp_driver_tsens
                  esp_http_client esp-tls mbedtls
                  board_power_bsp epaper_driver_bsp)

# la version annoncee par la carte est celle du fichier VERSION
file(READ "${CMAKE_CURRENT_LIST_DIR}/../VERSION" OM_VER)
string(STRIP "${OM_VER}" OM_VER)
target_compile_definitions(${COMPONENT_LIB} PUBLIC OM_VERSION="${OM_VER}")
# changer VERSION relance cmake ET la compilation (sinon la carte garde
# l'ancienne chaine : constate en testant 9.9.9, qui n'arrivait pas)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${CMAKE_CURRENT_LIST_DIR}/../VERSION")
