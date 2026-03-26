#include "led.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "esp_log.h"
#include <math.h>

#define LED_GPIO 8
#define ALARM_DURATION_MS (3 * 60 * 1000)
#define FLASH_PERIOD_MS 200
#define PULSE_PERIOD_MS 2000
#define MAX_BRIGHTNESS 40

static const char *TAG = "led";
static led_strip_handle_t strip;
static volatile bool alarming = false;
static volatile uint32_t alarm_start_tick = 0;

void led_init(void) {
    led_strip_config_t cfg = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags.invert_out = false,
    };
    led_strip_rmt_config_t rmt_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&cfg, &rmt_cfg, &strip));
    led_strip_clear(strip);
    ESP_LOGI(TAG, "neopixel ready on gpio %d", LED_GPIO);
}

void led_set_alarm(void) {
    alarm_start_tick = xTaskGetTickCount();
    alarming = true;
    ESP_LOGW(TAG, "alarm triggered");
}

void led_set_idle(void) {
    alarming = false;
}

static void show(uint8_t r, uint8_t g, uint8_t b) {
    led_strip_set_pixel(strip, 0, r, g, b);
    led_strip_refresh(strip);
}

void led_task(void *arg) {
    uint32_t tick = 0;
    while (1) {
        if (alarming) {
            uint32_t elapsed = (xTaskGetTickCount() - alarm_start_tick)
                               * portTICK_PERIOD_MS;
            if (elapsed >= ALARM_DURATION_MS) {
                alarming = false;
                ESP_LOGI(TAG, "alarm expired, back to idle");
            } else {
                bool on = ((tick / (FLASH_PERIOD_MS / 20)) % 2) == 0;
                show(on ? MAX_BRIGHTNESS : 0, 0, 0);
            }
        } else {
            float phase = (float)(tick % (PULSE_PERIOD_MS / 20))
                          / (float)(PULSE_PERIOD_MS / 20);
            float bright = (sinf(phase * 2.0f * M_PI) + 1.0f) / 2.0f;
            uint8_t g = (uint8_t)(bright * MAX_BRIGHTNESS);
            show(0, g, 0);
        }
        tick++;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
