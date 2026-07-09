#ifndef AUDIO_MESSAGE_H
#define AUDIO_MESSAGE_H

#include <stdint.h>
#include <stdbool.h>
#include "joyinside_time.h"

#define USE_OPUS_ENCODE 1

// 音频编码类型
typedef enum {
    AUDIO_CODEC_PCM,
    AUDIO_CODEC_OPUS
} audio_codec_type;

// 音频消息结构
typedef struct {
    char mid[UUID_STRING_LEN+1];              // 消息唯一标识符
    char *uid;              // 用户ID
    char *send_buffer;
    int send_buffer_len;
    int index;
} audio_message_t;

/**
 * @brief 创建音频消息
 * @param msg 音频消息结构指针
 * @param uid 用户ID
 * @return audio_message_t * handler, 成功返回指针句柄，失败返回NULL
 */
audio_message_t * audio_message_create(const char *uid);

/**
 * @brief 发送音频数据
 * @param client WebSocket客户端
 * @param msg 音频消息结构指针
 * @param audio_data 音频数据
 * @param data_len 数据长度
 * @return 0成功，非0失败
 */
int audio_message_send(void *ctx, audio_message_t *msg, 
                      const uint8_t *audio_data, size_t data_len);

int audio_finish_event_send(void *ctx, audio_message_t *msg);

int audio_init_event_send(void *ctx, audio_message_t *msg);

int audio_ping_send(void *ctx, audio_message_t *msg);

int audio_interrupt_send(void *ctx, audio_message_t *msg);

int audio_text_input_send(audio_message_t *msg, char *text);

int audio_text_to_speech(audio_message_t *msg, char *text);

/**
 * @brief 释放音频消息资源
 * @param msg 音频消息结构指针
 */
void audio_message_free(audio_message_t *msg);

void audio_message_reset(audio_message_t *msg);

#endif // AUDIO_MESSAGE_H
