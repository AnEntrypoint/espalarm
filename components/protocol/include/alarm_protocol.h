#pragma once

#include <stdint.h>

#define ALARM_UDP_PORT 4210
#define ALARM_AP_SSID "espalarm"
#define ALARM_AP_PASS "espalarm123"
#define ALARM_AP_CHANNEL 1
#define ALARM_AP_MAX_CONN 8
#define ALARM_AP_IP "192.168.4.1"

#define ALARM_MSG_MAGIC 0xA1A2
#define ALARM_MSG_MOTION 0x01
#define ALARM_MSG_HEARTBEAT 0x02
#define ALARM_MSG_ACK 0x03

typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint8_t type;
    uint8_t sensor_id;
    uint32_t uptime_ms;
} alarm_msg_t;
