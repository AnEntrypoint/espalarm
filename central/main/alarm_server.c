#include "alarm_server.h"
#include "led.h"
#include "alarm_protocol.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "lwip/sockets.h"
#include <string.h>

#define BUZZER_GPIO 3
#define ALARM_DURATION_MS (3 * 60 * 1000)

static const char *TAG = "alarm_srv";
static volatile bool buzzer_active = false;
static volatile uint32_t buzzer_start_tick = 0;

void alarm_server_init(void) {
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << BUZZER_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    gpio_set_level(BUZZER_GPIO, 0);
    ESP_LOGI(TAG, "buzzer relay ready on gpio %d", BUZZER_GPIO);
}

static void trigger_alarm(uint8_t sensor_id) {
    ESP_LOGW(TAG, "motion from sensor %d", sensor_id);
    led_set_alarm();
    buzzer_active = true;
    buzzer_start_tick = xTaskGetTickCount();
    gpio_set_level(BUZZER_GPIO, 1);
}

void alarm_server_task(void *arg) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "socket failed");
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(ALARM_UDP_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "bind failed");
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "listening on udp port %d", ALARM_UDP_PORT);
    alarm_msg_t msg;
    struct sockaddr_in src;
    socklen_t src_len = sizeof(src);

    while (1) {
        if (buzzer_active) {
            uint32_t elapsed = (xTaskGetTickCount() - buzzer_start_tick)
                               * portTICK_PERIOD_MS;
            if (elapsed >= ALARM_DURATION_MS) {
                buzzer_active = false;
                gpio_set_level(BUZZER_GPIO, 0);
                ESP_LOGI(TAG, "buzzer off");
            }
        }

        struct timeval tv = { .tv_sec = 0, .tv_usec = 100000 };
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        int len = recvfrom(sock, &msg, sizeof(msg), 0,
                           (struct sockaddr *)&src, &src_len);
        if (len < (int)sizeof(msg)) continue;
        if (msg.magic != ALARM_MSG_MAGIC) continue;

        switch (msg.type) {
        case ALARM_MSG_MOTION:
            trigger_alarm(msg.sensor_id);
            break;
        case ALARM_MSG_HEARTBEAT:
            ESP_LOGI(TAG, "heartbeat from sensor %d, up %lums",
                     msg.sensor_id, (unsigned long)msg.uptime_ms);
            break;
        }
    }
}
