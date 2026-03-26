# espalarm

Wireless PIR alarm system using ESP32-C2 modules.

## Architecture

- **Central node** (ESP32-C2 dev board with NeoPixel): Runs WiFi AP, listens for motion alerts via UDP, flashes red LED + activates buzzer relay for 3 minutes, then pulses green
- **Sensor nodes** (ESP32-C2 Mini with HC-SR505 PIR): Connect to central AP, send UDP packets on motion detection

## Hardware

### Central
- ESP32-C2 dev board (GPIO8 = WS2812 NeoPixel)
- Relay module on GPIO3 (buzzer/siren)

### Sensor
- ESP32-C2 Mini
- HC-SR505 PIR sensor on GPIO4
- Power via USB

## Building

Requires ESP-IDF v5.3+. Binaries are built automatically via GitHub Actions.

### Local build
```bash
cd central  # or sensor
idf.py set-target esp32c2
idf.py build
idf.py flash
```

## Configuration

WiFi credentials and GPIO pins are defined in `components/protocol/include/alarm_protocol.h`.

Default AP: `espalarm` / `espalarm123`
