| Supported Targets | ESP32-S3 |
| ----------------- | -------- |

# DuetAI-S3

DuetAI-S3 is an ESP32-S3 voice assistant firmware. It runs on-device wake word
detection, plays a voice prompt on wake, drives an LVGL GUI on a round LCD, and
ships the networking HAL (TCP/TLS/HTTP/WebSocket) used to talk to a cloud AI
backend.

The wake word is "你好东东" (`nihaodongdong`). On detection the firmware plays a
local acknowledgement prompt and (going forward) will open a session to the
cloud for streaming speech interaction.

## Features

- On-device wake word detection via Espressif esp-sr (WakeNet).
- Dual-microphone audio capture through an ES7210 ADC over I2S.
- Local WAV prompt playback through the board codec.
- LVGL v9.5 GUI on a GC9A01 round LCD with touch input.
- Networking HAL: TCP, TLS (mbedTLS), HTTP, and WebSocket client
  (`components/hal_socket`).
- OTA-ready dual-app partition layout.

## Hardware

Target board is an ESP32-S3 module with:

- ES7210 multi-channel audio ADC (dual MEMS microphones over I2S).
- Audio codec + speaker for prompt/response playback.
- GC9A01 240x240 round LCD.
- Capacitive touch panel.
- ICM42670 IMU and ALS1206AC ambient light sensor (I2C).

## Repository layout

```
├── main/                     App entry, LVGL port, wake word + audio tasks
├── components/
│   ├── bsp/                  Board support: LCD, codec, I2S, I2C, GPIO, ADC
│   ├── hal_socket/           TCP/TLS/HTTP/WebSocket networking HAL
│   ├── os_hal/               OS abstraction (FreeRTOS), logging, event loop
│   ├── i2c_devices/          IMU (icm42670), touch panel, ALS1206AC
│   ├── i2c_bus/              I2C bus driver
│   ├── lvgl/                 LVGL v9.5 graphics library
│   ├── PageManager_C/        LVGL page/router manager
│   ├── iot_button/           Button handling
│   ├── littlefs/             LittleFS filesystem
│   └── finsh/                Shell/console
├── voice/                    Voice prompt assets (SPIFFS "voice" partition)
├── partitions.csv            Partition table (dual OTA + voice/model SPIFFS)
├── rules/                    Project coding, API, and docs conventions
└── sdkconfig                 Project configuration
```

The `voice` and `model` SPIFFS partitions hold the WAV prompts and the packed
esp-sr `srmodels.bin`, respectively (see [partitions.csv](partitions.csv)).

## Build and flash

Requires ESP-IDF v5.5.x (see [dependencies.lock](dependencies.lock)).

```
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

## Roadmap

The current firmware covers local wake-up, display, and audio. Planned work,
roughly in priority order:

1. Cloud session over WebSocket
   - Wire the wake word callback to open a `hal_ws_connect` session to the AI
     backend and stream captured audio upstream.
   - Handle the new `hal_ws_read` return codes (`HAL_WS_CLOSED`,
     `HAL_WS_CONTROL`, `HAL_WS_ERR`) and reconnect logic in the session layer.
2. Streaming speech pipeline
   - Continuous capture and upload after wake, plus playback of the streamed
     response through the audio player.
   - Barge-in / VAD to end an utterance without a fixed timeout.
3. WebSocket protocol hardening
   - Support fragmented frames (currently a single frame per read; fragmented
     frames are logged and treated as standalone).
   - Randomize the client masking key and generate a per-connection
     `Sec-WebSocket-Key` instead of the fixed constant.
4. GUI states
   - Assistant UI states (idle / listening / thinking / speaking) via
     PageManager_C, driven by the session layer.
5. Provisioning and connectivity
   - Wi-Fi provisioning flow and network status surfaced on the LCD.
6. OTA updates
   - Firmware update over the existing dual-app (`ota_0` / `ota_1`) layout.

## Conventions

Coding style, API design, commit message, and documentation conventions live in
[rules/](rules/). Follow them for contributions.
