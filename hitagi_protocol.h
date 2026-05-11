/*
 * About:
 *   Shared Motorola Flash Protocol declarations.
 *
 * License:
 *   MIT
 */

#ifndef HITAGI_PROTOCOL_H
#define HITAGI_PROTOCOL_H

#include "hitagi_platform.h"
#include "flash.h"

extern const HITAGI_CMD_TABLE_T hitagi_cmd_tbl[];
extern const u8 hitagi_cmd_tbl_size;

extern u16 *received_address_ptr;
extern u16  received_packet_size;
extern u8   rx_command[MAX_COMMAND_STR_SIZE];
extern u8  *rx_data;
extern u8  *tx_data;

void util_u8_to_hexasc(u8 val, u8 *str);
void util_u16_to_hexasc(u16 val, u8 *str);
void util_u32_to_hexasc(u32 val, u8 *str);
u32 util_hexasc_to_u32(const u8 *str, u8 size);
void util_string_copy(u8 *dst, const u8 *src);

void hitagi_send_packet(const u8 *cmd, const u8 *data);
void hitagi_send_bin_packet(const u8 *cmd, const u8 *data, u16 bin_size);
void hitagi_send_ack(const u8 *data);
void hitagi_send_error(u8 error_code);

#endif /* !HITAGI_PROTOCOL_H */
