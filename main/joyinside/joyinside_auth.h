/*
 * SPDX-FileCopyrightText: 2026 JD AIoT
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __JOYINSIDE_AUTH_H__
#define __JOYINSIDE_AUTH_H__

#ifdef __cplusplus
extern "C" {
#endif

#define JOYINSIDE_UUID_STR_LEN  (36)

/**
 * @brief  Generate a random UUID string
 *
 * @param[out]  out  Buffer of at least JOYINSIDE_UUID_STR_LEN + 1 bytes
 */
void joyinside_auth_gen_uuid(char *out);

/**
 * @brief  Build the full JoyInside WebSocket connect URI with signed auth params
 *
 *         Produces "<base_uri>?botId=...&accessSign=..." where the signature is
 *         HMAC-MD5 over the sorted auth fields (access version V2).
 *
 * @param[in]  base_uri            Endpoint URI, e.g. "wss://joyinside.jd.com/soulmate/voiceCall/v4"
 * @param[in]  access_key_id       Application access key id
 * @param[in]  access_key_secret   Application access key secret
 * @param[in]  bot_id              Robot / bot identifier
 *
 * @return  Heap-allocated URI string (caller frees), or NULL on error
 */
char *joyinside_auth_build_uri(const char *base_uri, const char *access_key_id,
                               const char *access_key_secret, const char *bot_id);

/**
 * @brief  Compute the device JWT (HS256) used inside the chat-update event
 *
 * @param[in]   device_id     Device id (did)
 * @param[in]   device_token  Device token used as the HMAC key
 * @param[out]  jwt_out       Buffer receiving the NUL-terminated JWT
 * @param[in]   jwt_out_len   Size of jwt_out
 *
 * @return  0 on success, -1 on error
 */
int joyinside_auth_build_jwt(const char *device_id, const char *device_token,
                             char *jwt_out, int jwt_out_len);

#ifdef __cplusplus
}
#endif

#endif /* __JOYINSIDE_AUTH_H__ */
