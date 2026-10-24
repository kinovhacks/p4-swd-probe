idf_component_register(SRCS "main.c" "../vendor/DAP.c" "../vendor/SW_DP.c"
 INCLUDE_DIRS "." "../vendor" REQUIRES esp_driver_uart esp_driver_gpio esp_timer spi_flash)
