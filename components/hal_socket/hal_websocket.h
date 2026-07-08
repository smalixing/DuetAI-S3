#ifndef __HAL_WEBSOCKET_H__
#define __HAL_WEBSOCKET_H__

#include "hal_tls.h"

/* hal_ws_read return codes.
 * >= 0            : application data frame received, value is payload byte count (0 = empty data frame)
 * HAL_WS_CLOSED   : peer sent a CLOSE frame; connection is closing
 * HAL_WS_CONTROL  : a control frame (PING/PONG) was handled internally, no app data
 * HAL_WS_ERR      : read/protocol error
 */
#define HAL_WS_ERR      (-1)
#define HAL_WS_CLOSED   (-2)
#define HAL_WS_CONTROL  (-3)

/* WebSocket frame opcodes reported by hal_ws_read_ex via its opcode out-param */
#define HAL_WS_OPCODE_TEXT      0x01
#define HAL_WS_OPCODE_BINARY    0x02

void *hal_ws_connect(const char *url, int opt_port, const tls_param_t *tls_param, uint32_t timeout_ms);
void hal_ws_disconnect(void *ctx);
int hal_ws_write(void *ctx, const uint8_t *buf, int len, uint32_t timeout_ms);
int hal_ws_write_text(void *ctx, const uint8_t *buf, int len, uint32_t timeout_ms);
int hal_ws_read(void *ctx, uint8_t *buf, int len, uint32_t timeout_ms);

/* Same as hal_ws_read, but on a data frame writes the frame opcode
 * (HAL_WS_OPCODE_TEXT / HAL_WS_OPCODE_BINARY) to *opcode when opcode is non-NULL.
 * Lets callers distinguish TEXT (JSON) from BINARY (audio) frames. */
int hal_ws_read_ex(void *ctx, uint8_t *buf, int len, int *opcode, uint32_t timeout_ms);

int hal_ws_send_close_frame(void *ctx, uint32_t timeout_ms);
int hal_ws_send_ping_frame(void *ctx, uint32_t timeout_ms);

#endif
