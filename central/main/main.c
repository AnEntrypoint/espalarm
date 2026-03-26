#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "alarm_protocol.h"
#include "led.h"
#include "alarm_server.h"

static const char *TAG = "central";

static void wifi_ap_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));

    wifi_config_t ap_cfg = {
        .ap = {
            .ssid = ALARM_AP_SSID,
            .ssid_len = sizeof(ALARM_AP_SSID) - 1,
            .password = ALARM_AP_PASS,
            .channel = ALARM_AP_CHANNEL,
            .max_connection = ALARM_AP_MAX_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "ap started: ssid=%s ch=%d", ALARM_AP_SSID, ALARM_AP_CHANNEL);
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());

    led_init();
    alarm_server_init();
    wifi_ap_init();

    xTaskCreate(led_task, "led", 2048, NULL, 5, NULL);
    xTaskCreate(alarm_server_task, "alarm_srv", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "central node running");
}
