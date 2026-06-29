#ifndef __HAL_UDP_H__
#define __HAL_UDP_H__

// client
int HAL_UDP_Connect(const char *host, unsigned short port);
void HAL_UDP_Disconnect(int fd);
int HAL_UDP_Write(int fd, const unsigned char *p_data, unsigned int datalen);
int HAL_UDP_Read(int fd, unsigned char *p_data, unsigned int datalen);
int HAL_UDP_ReadTimeout(int fd, unsigned char *p_data, unsigned int datalen, unsigned int timeout_ms);


// server
int HAL_UDP_CreateBind(const char *host, unsigned short port);
int HAL_UDP_ReadTimeoutPeerInfo(int fd, unsigned char *p_data, unsigned int datalen, unsigned int timeout_ms,
                                char *recv_ip_addr, unsigned char recv_addr_len, unsigned short *recv_port);
int HAL_UDP_WriteTo(int fd, const unsigned char *p_data, unsigned int datalen, char *host, unsigned short port);
void HAL_UDP_Close(int fd);

#endif
