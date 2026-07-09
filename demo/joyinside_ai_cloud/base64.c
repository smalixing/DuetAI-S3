#include "base64.h"
#include <string.h>
#include <stdint.h>

#include <stdio.h>

static const unsigned char base64_enc_map[64] =
{
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
    'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
    'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd',
    'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x',
    'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', '+', '/'
};

static const unsigned char base64_dec_map[128] =
{
    127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
    127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
    127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
    127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
    127, 127, 127,  62, 127, 127, 127,  63,  52,  53,
     54,  55,  56,  57,  58,  59,  60,  61, 127, 127,
    127,  64, 127, 127, 127,   0,   1,   2,   3,   4,
      5,   6,   7,   8,   9,  10,  11,  12,  13,  14,
     15,  16,  17,  18,  19,  20,  21,  22,  23,  24,
     25, 127, 127, 127, 127, 127, 127,  26,  27,  28,
     29,  30,  31,  32,  33,  34,  35,  36,  37,  38,
     39,  40,  41,  42,  43,  44,  45,  46,  47,  48,
     49,  50,  51, 127, 127, 127, 127, 127
};

size_t joyinside_base64_encode_len(size_t input_length)
{
    size_t n = input_length;
    return (n + 2) / 3 * 4 + 1;
}

int joyinside_base64_encode(unsigned char *dst, size_t dlen, const unsigned char *src, size_t slen)
{
    size_t i, n;
    int C1, C2, C3;
    unsigned char *p;

    if (slen == 0) {
        *dst = '\0';
        return 0;
    }

    n = (slen + 2) / 3 * 4;

    if (dlen < n + 1) {
        printf("dlen = %d, n = %d\n", dlen, n);
        return -1;
    }

    n = (slen / 3) * 3;
    p = dst;

    for (i = 0; i < n; i += 3) {
        C1 = *src++;
        C2 = *src++;
        C3 = *src++;

        *p++ = base64_enc_map[(C1 >> 2) & 0x3F];
        *p++ = base64_enc_map[(((C1 &  3) << 4) + (C2 >> 4)) & 0x3F];
        *p++ = base64_enc_map[(((C2 & 15) << 2) + (C3 >> 6)) & 0x3F];
        *p++ = base64_enc_map[C3 & 0x3F];
    }

    if (i < slen) {
        C1 = *src++;
        *p++ = base64_enc_map[(C1 >> 2) & 0x3F];

        if (i + 1 < slen) {
            C2 = *src++;
            *p++ = base64_enc_map[(((C1 & 3) << 4) + (C2 >> 4)) & 0x3F];
            *p++ = base64_enc_map[((C2 & 15) << 2) & 0x3F];
        } else {
            *p++ = base64_enc_map[((C1 & 3) << 4) & 0x3F];
            *p++ = '=';
        }
        *p++ = '=';
    }

    *p = '\0';

    return 0;
}

size_t joyinside_base64_decode_len(size_t input_length)
{
    return (input_length / 4) * 3 + 1;
}

int joyinside_base64_decode(unsigned char *dst, size_t dlen, const unsigned char *src, size_t slen)
{
    size_t i, n;
    uint32_t j, x;
    unsigned char *p;

    // 去掉末尾的填充字符
    while (slen > 0 && src[slen - 1] == '=') {
        slen--;
    }

    // 检查输入长度是否为4的倍数
    if ((slen & 3) != 0) {
        return -1;
    }

    if (slen == 0) {
        *dst = '\0';
        return 0;
    }

    n = ((slen >> 2) * 3);
    if (dlen < n + 1) {
        return -1;
    }

    n = slen >> 2;
    p = dst;

    for (i = 0; i < n; i++) {
        // 获取4个字符并验证
        for (x = 0, j = 0; j < 4; j++) {
            unsigned char c = src[i * 4 + j];
            if (c >= 128 || base64_dec_map[c] == 127) {
                return -1;
            }
            x = (x << 6) | (base64_dec_map[c] & 0x3F);
        }

        // 写入解码后的3个字节
        *p++ = (unsigned char)(x >> 16);
        *p++ = (unsigned char)(x >> 8);
        *p++ = (unsigned char)x;
    }

    // 处理末尾的填充
    j = 0;
    if (src[slen - 1] == '=') j++;
    if (src[slen - 2] == '=') j++;

    *p = '\0';
    return n * 3 - j;
}