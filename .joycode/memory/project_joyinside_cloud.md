---
name: JoyInside 云连接鉴权与时间同步
description: 'DuetAI-S3 设备连接 JoyInside 云 (wss://joyinside.jd.com) 的鉴权机制与 SNTP 时间同步依赖'
type: project
---

设备通过 WebSocket (wss://joyinside.jd.com/soulmate/voiceCall/v4) 连接 JoyInside 云,鉴权采用 V2 签名(HMAC-MD5),URI query 携带 accessTimestamp(毫秒 wall-clock)等参数,由 `main/joyinside/joyinside_auth.c` 的 `joyinside_auth_build_uri` 构造。

**Why:** 服务端会校验 accessTimestamp 与真实时间的偏差,偏差过大直接返回 HTTP 401 Unauthorized(表现为 WebSocket 握手响应无 Sec-WebSocket-Accept)。ESP32 设备上电后系统时间停在 1970,若未同步会必然 401。

**How to apply:** 发起云连接前必须先经 SNTP 同步系统时间。已在 `main/wifi_sta.c` 新增 `wifi_sta_sync_time()`,并在 `main/main.c` 的 WiFi 连接成功后、`joyinside_session_start()` 前调用。排查云连接 401 时优先确认设备时间是否已同步。签名算法与官方参考实现 `demo/joyinside_ai_cloud/hmac_signature.c` 一致。