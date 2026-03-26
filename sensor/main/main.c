#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "lwip/sockets.h"
#include "alarm_protocol.h"
#include <string.h>

#define PIR_GPIO 4
#define DEBOUNCE_MS 5000
#define HEARTBEAT_INTERVAL_MS 30000

static const char *TAG = "sensor";
static volatile bool wifi_connected = false;

static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data) {
    if (base == WIFI_EVENT) {
        switch (id) {
        case WIFI_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            wifi_connected = false;
            ESP_LOGW(TAG, "disconnected, reconnecting...");
            esp_wifi_connect();
            break;
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *evt = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "got ip: " IPSTR, IP2STR(&evt->ip_info.ip));
        wifi_connected = true;
    }
}

static void wifi_sta_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t sta_cfg = {
        .sta = {
            .ssid = ALARM_AP_SSID,
            .password = ALARM_AP_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "connecting to %s", ALARM_AP_SSID);
}

static uint8_t get_sensor_id(void) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    return mac[5];
}

static int send_msg(int sock, const alarm_msg_t *msg) {
    struct sockaddr_in dest = {
        .sin_family = AF_INET,
        .sin_port = htons(ALARM_UDP_PORT),
    };
    inet_pton(AF_INET, ALARM_AP_IP, &dest.sin_addr);
    return sendto(sock, msg, sizeof(*msg), 0,
                  (struct sockaddr *)&dest, sizeof(dest));
}

static void pir_init(void) {
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << PIR_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    ESP_LOGI(TAG, "pir ready on gpio %d", PIR_GPIO);
}

static void sensor_task(void *arg) {
    uint8_t sid = get_sensor_id();
    ESP_LOGI(TAG, "sensor id: 0x%02x", sid);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "socket failed");
        vTaskDelete(NULL);
        return;
    }

    uint32_t last_motion_ms = 0;
    uint32_t last_heartbeat_ms = 0;

    while (1) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        if (wifi_connected) {
            if (gpio_get_level(PIR_GPIO) == 1
                && (now - last_motion_ms) > DEBOUNCE_MS) {
                last_motion_ms = now;
                alarm_msg_t msg = {
                    .magic = ALARM_MSG_MAGIC,
                    .type = ALARM_MSG_MOTION,
                    .sensor_id = sid,
                    .uptime_ms = now,
                };
                send_msg(sock, &msg);
                ESP_LOGW(TAG, "motion detected, sent alarm");
            }

            if ((now - last_heartbeat_ms) > HEARTBEAT_INTERVAL_MS) {
                last_heartbeat_ms = now;
                alarm_msg_t msg = {
                    .magic = ALARM_MSG_MAGIC,
                    .type = ALARM_MSG_HEARTBEAT,
                    .sensor_id = sid,
                    .uptime_ms = now,
                };
                send_msg(sock, &msg);
                ESP_LOGI(TAG, "heartbeat sent");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    pir_init();
    wifi_sta_init();
    xTaskCreate(sensor_task, "sensor", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "sensor node running");
}
