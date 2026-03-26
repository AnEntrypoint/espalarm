idf_component_register(
    SRCS "main.c" "led.c" "alarm_server.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_wifi esp_netif driver led_strip protocol
)
