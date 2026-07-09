#ifndef BASE64_H
#define BASE64_H

#include <stddef.h>
#include <stdint.h>

/**
 * 计算Base64编码后的长度
 * @param input_length 输入数据长度
 * @return 编码后的长度
 */
size_t joyinside_base64_encode_len(size_t input_length);

/**
 * Base64编码
 * @param dst 目标缓冲区
 * @param dlen 目标缓冲区长度
 * @param src 源数据
 * @param slen 源数据长度
 * @return 成功返回0，失败返回-1
 */
int joyinside_base64_encode(unsigned char *dst, size_t dlen, const unsigned char *src, size_t slen);

/**
 * 计算Base64解码后的长度
 * @param input_length 输入数据长度
 * @return 解码后的长度
 */
size_t joyinside_base64_decode_len(size_t input_length);

/**
 * Base64解码
 * @param dst 目标缓冲区
 * @param dlen 目标缓冲区长度
 * @param src 源数据
 * @param slen 源数据长度
 * @return 成功返回实际解码长度，失败返回-1
 */
int joyinside_base64_decode(unsigned char *dst, size_t dlen, const unsigned char *src, size_t slen);

#endif // BASE64_H