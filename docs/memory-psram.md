# 内存与 PSRAM 使用规范

本项目运行在 ESP32-S3，内部 RAM（DIRAM，约 341 KB）紧张，模组带 8 MB PSRAM。
内部 RAM 占用过高会在 WiFi/TLS 峰值时导致分配失败与不稳定，因此大缓冲应尽量放入 PSRAM。

## 背景

- DIRAM 总量约 341 KB，静态 `.bss` 已占约 86 KB，开机静态占用约 64%。
- 运行时余量会被 WiFi、TLS/AES、opus 等峰值挤压。
- 历史事故：将 I2S `dma_buf_count` 从 6 调到 32，榨干内部 RAM，导致 `esp-aes: Failed to allocate memory`、TLS 握手失败。

## 关键配置

`sdkconfig` 中：

```
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_USE_MALLOC=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384
CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=32768
```

`ALWAYSINTERNAL=16384` 表示：只有单次分配 ≥ 16 KB 时才可能自动落入 PSRAM，
小于该阈值的 `malloc` 默认仍在内部 RAM。因此中小缓冲若要进 PSRAM 必须显式指定。

## 规则

1. 大的、生命周期长的缓冲显式用 PSRAM 分配：

   ```c
   #include "esp_heap_caps.h"

   buf = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
   obj = heap_caps_calloc(1, sizeof(*obj), MALLOC_CAP_SPIRAM);
   ```

   `heap_caps_malloc` 分配的内存用普通 `free()` 释放即可。

2. 任务栈从 PSRAM 分配：通过 `hal_thread_create`（内部使用
   `xTaskCreatePinnedToCoreWithCaps(..., MALLOC_CAP_SPIRAM)`）。

3. DMA 安全性：喂给 legacy `i2s_read` / `i2s_write` 的用户缓冲可以放 PSRAM。
   legacy I2S 驱动在其内部 DMA 描述符与用户缓冲之间做 memcpy，用户缓冲无需
   DMA-capable（内部）内存。真正的 DMA 缓冲是驱动内部由 `dma_buf_count` 决定的
   那部分，始终位于内部 RAM，不要通过加大它来解决抖动而耗尽内部 RAM。

4. 不需要迁移到 PSRAM 的情况：

   - 短命的小分配（鉴权临时字符串、几 KB 的 JSON 发送缓冲、编解码器 struct）。
     收益小，且会给连接/热路径增加 PSRAM 访问延迟。
   - opus 库内部编解码器状态（解码器约 18 KB、编码器更大）超过 ALWAYSINTERNAL
     阈值，会自动进入 PSRAM。

## 已迁移到 PSRAM 的缓冲

- `main/joyinside/joyinside.c`：RX/TX 任务的文本重组缓冲（2 × 16 KB）、
  `struct joyinside`、解码 PCM 缓冲、TX opus_buf/frame。
- `main/wake_word_task.c`：I2S 采集缓冲 `i2s_buffer`、模型输入 `mono_buffer`。
- `main/audio_player.c`：WAV 播放与流式 PCM 的 mono/stereo 暂存缓冲。

## 排查方法

- 开机日志的 Memory Type Usage Summary，关注 DIRAM 的使用百分比。
- 运行时 `esp_get_free_internal_heap_size()` 观察内部 RAM 空闲。
- 若出现 AES/TLS 分配失败，优先怀疑内部 RAM 被大缓冲或过深 DMA 缓冲占用。
