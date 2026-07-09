#ifndef WS_PARAMS_H
#define WS_PARAMS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 生成WebSocket认证参数字符串
 * 
 * @param accessKeyID 应用标识
 * @param accessKeySecret 应用密钥
 * @param botid 机器人标识
 * @param version 认证版本
 * @return char* 返回生成的认证参数字符串，调用者负责释放内存。失败返回NULL
 */
char* ws_generate_auth_params(const char *accessKeyID,     // 应用标识
                            const char *accessKeySecret,   // 应用密钥
                            const char *botid,            // 机器人标识
                            const char *version,          // 认证版本, 
                            char access_sign[33]);

#ifdef __cplusplus
}
#endif

#endif /* WS_PARAMS_H */