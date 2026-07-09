#ifndef __JOYINSIDE_HMA_H
#define __JOYINSIDE_HMA_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "mbedtls/md.h"

#include "joyinside_err.h"
#include "joyinside_log.h"


joyinside_err_t hmac_sha256_sign(const unsigned char *key, size_t key_len, const unsigned char *data, size_t data_len, uint8_t *output);

joyinside_err_t hmac_md5_sign(const unsigned char *key, size_t key_len, const unsigned char *data, size_t data_len, uint8_t *output);

char* bytes_to_hex(const uint8_t* bytes, int length);

int md5_sum(char *data, int len, char hash[33]);

#endif