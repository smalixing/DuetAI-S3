| Supported Targets | ESP32-S3 |
| ----------------- | -------- |

# DuetAI-S3

DuetAI-S3 is an ESP32-S3 voice assistant firmware. It runs on-device wake word
detection, connects to Wi-Fi, streams speech to the JD JoyInside cloud over a
WebSocket, plays back the cloud's TTS response, and drives an LVGL GUI on a round
LCD. It ships the networking HAL (TCP/TLS/HTTP/WebSocket) used to talk to the
cloud AI backend.

The wake word is "你好东东" (`nihaodongdong`). On detection the firmware plays a
local acknowledgement prompt, opens a JoyInside voice turn, uplinks the captured
microphone audio, and plays the streamed cloud response.

## Features

- On-device wake word detection via Espressif esp-sr (WakeNet).
- Dual-microphone audio capture through an ES7210 ADC over I2S.
- Wi-Fi station connectivity with auto-reconnect.
- JD JoyInside cloud voice call: opus audio uplink and streaming TTS playback
  over a WebSocket (`main/joyinside`).
- Local WAV prompt playback and streaming PCM playback through the board codec.
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
├── main/                     App entry, Wi-Fi, wake word, audio, JoyInside client
│   └── joyinside/            JD JoyInside cloud voice-call module
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

## Configuration

Before building, set the network and cloud credentials:

- **Wi-Fi**: edit the `WIFI_SSID` and `WIFI_PSK` macros at the top of
  [main/wifi_sta.c](main/wifi_sta.c).
- **JoyInside cloud**: run `idf.py menuconfig` → *JoyInside Cloud Voice Call* and
  fill in the access key id/secret, bot id, device token, and device id (or edit
  the defaults in [main/joyinside/Kconfig.projbuild](main/joyinside/Kconfig.projbuild)).

Wi-Fi and the cloud session are best-effort: if either fails to come up, local
wake-word detection and prompt playback still work.

## Build and flash

Requires ESP-IDF v5.5.x (see [dependencies.lock](dependencies.lock)).

```
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

## Roadmap

The current firmware covers local wake-up, display, audio, Wi-Fi, and a full
JoyInside cloud voice turn (uplink + TTS playback). Planned work, roughly in
priority order:

1. Streaming speech refinements
   - Barge-in / VAD to end an utterance without relying solely on the cloud's
     `ASR_FINAL`, and to interrupt playback when the user speaks.
   - Acoustic echo cancellation so playback does not feed back into the uplink.
2. WebSocket protocol hardening
   - Support fragmented frames (currently a single frame per read; fragmented
     frames are logged and treated as standalone).
   - Randomize the client masking key and generate a per-connection
     `Sec-WebSocket-Key` instead of the fixed constant.
3. GUI states
   - Assistant UI states (idle / listening / thinking / speaking) via
     PageManager_C, driven by the session layer.
4. Provisioning and connectivity
   - Wi-Fi provisioning flow (replacing the compile-time SSID/PSK macros) and
     network status surfaced on the LCD.
5. OTA updates
   - Firmware update over the existing dual-app (`ota_0` / `ota_1`) layout.

## Conventions

Coding style, API design, commit message, and documentation conventions live in
[rules/](rules/). Follow them for contributions.
