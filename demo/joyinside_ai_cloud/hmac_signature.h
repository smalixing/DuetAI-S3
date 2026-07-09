#ifndef _HMAC_SIGNATURE
#define _HMAC_SIGNATURE

int joyinside_cloud_signature(char *accessKeyID, 
        char *accessKeySecret, 
        char *accessNonce, 
        char *accessTimestamp, 
        char *botId, 
        char hexdigest[33]);

#endif