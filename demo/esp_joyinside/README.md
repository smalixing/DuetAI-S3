# esp_joyinside

Lightweight JoyInside `voiceCall/v4` voice chat component for ESP-IDF.

## Scope

- WebSocket session lifecycle (`init` / `start` / `stop` / `deinit`)
- V2 HMAC authentication for `voiceCall/v4`
- `CLIENT_VOICE_CHAT_UPDATE`, `CLIENT_AUDIO_FINISH`, `CLIENT_INTERRUPT`
- Binary uplink audio and downlink ASR / EVENT / TTS handling
- HTTP device bootstrap (`getToken` + `device/register`)

## Public headers

| Header | Purpose |
|--------|---------|
| `esp_joyinside_chat.h` | Voice chat session API |
| `esp_joyinside_bootstrap.h` | Device registration API |

JWT generation is internal to the component and used when `device_token` is set in the chat config.

## Examples

| Example | Description |
|---------|-------------|
| `examples/joyinside_basic_test` | Protocol validation without board audio |
| `examples/joyinside_ws_app` | Full duplex voice chat with board codec |

## Quick start

```c
esp_joyinside_chat_config_t config = ESP_JOYINSIDE_CHAT_DEFAULT_CONFIG();
config.bot_id = bot_id;
config.access_key_id = access_key_id;
config.access_key_secret = access_key_secret;
config.device_id = device_id;
config.event_callback = on_event;
config.audio_callback = on_audio;

esp_joyinside_chat_handle_t chat;
ESP_ERROR_CHECK(esp_joyinside_chat_init(&config, &chat));
ESP_ERROR_CHECK(esp_joyinside_chat_start(chat));
ESP_ERROR_CHECK(esp_joyinside_chat_send_audio_data(chat, pcm, pcm_len));
ESP_ERROR_CHECK(esp_joyinside_chat_send_audio_complete(chat));
ESP_ERROR_CHECK(esp_joyinside_chat_stop(chat));
ESP_ERROR_CHECK(esp_joyinside_chat_deinit(chat));
```

## Thread safety

Callbacks run in the `esp_websocket_client` task. Do not call chat APIs from callbacks unless documented otherwise. `event_data` and audio buffers are valid only for the callback duration.

## Configuration

Component Kconfig: `Component config -> ESP JoyInside Chat`

Bootstrap and credentials are passed at runtime through config structs. Persist `botId` in application storage after the first successful registration.
