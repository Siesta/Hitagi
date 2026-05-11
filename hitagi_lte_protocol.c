/*
 * About:
 *   Motorola Flash Protocol command handlers for Neptune LTE.
 *
 * License:
 *   MIT
 */

#include "hitagi_protocol.h"

static void hitagi_command_ADDR(const u8 *answer_str, const u8 *data_ptr, const u8 *buffer_next_byte);
static void hitagi_command_BIN(const u8 *answer_str, const u8 *data_ptr, const u8 *buffer_next_byte);
static void hitagi_command_ERASE(const u8 *answer_str, const u8 *data_ptr, const u8 *buffer_next_byte);
static void hitagi_command_READ(const u8 *answer_str, const u8 *data_ptr, const u8 *buffer_next_byte);
static void hitagi_command_RQHW(const u8 *answer_str, const u8 *data_ptr, const u8 *buffer_next_byte);
static void hitagi_command_RQRC(const u8 *answer_str, const u8 *data_ptr, const u8 *buffer_next_byte);
static void hitagi_command_RQVN(const u8 *answer_str, const u8 *data_ptr, const u8 *buffer_next_byte);
static void hitagi_command_RQSW(const u8 *answer_str, const u8 *data_ptr, const u8 *buffer_next_byte);
static void hitagi_command_RQSN(const u8 *answer_str, const u8 *data_ptr, const u8 *buffer_next_byte);
static void hitagi_command_RQFI(const u8 *answer_str, const u8 *data_ptr, const u8 *buffer_next_byte);
static void hitagi_command_READ_OTP(const u8 *answer_str, const u8 *data_ptr, const u8 *buffer_next_byte);
static void hitagi_command_RESTART(const u8 *answer_str, const u8 *data_ptr, const u8 *buffer_next_byte);
static void hitagi_command_POWER_DOWN(const u8 *answer_str, const u8 *data_ptr, const u8 *buffer_next_byte);
static void hitagi_lte_send_packet_aux(const u8 *cmd, const u8 *data, u16 bin_size);

static const u8 com_str[] = ",";
static const u8 rqsw_response[] = "Hitagi SW v1.0:Hitagi FLEX v1.0";

const HITAGI_CMD_TABLE_T hitagi_cmd_tbl[] = {
	{ (const u8 *) "ADDR",       (const u8 *) NULL,         hitagi_command_ADDR        },
	{ (const u8 *) "BIN",        (const u8 *) NULL,         hitagi_command_BIN         },
	{ (const u8 *) "ERASE",      (const u8 *) NULL,         hitagi_command_ERASE       },
	{ (const u8 *) "READ",       (const u8 *) "READ",       hitagi_command_READ        },
	{ (const u8 *) "RQHW",       (const u8 *) "RSHW",       hitagi_command_RQHW        },
#if !defined(FTR_COMPACT)
	{ (const u8 *) "RQRC",       (const u8 *) "RSRC",       hitagi_command_RQRC        },
	{ (const u8 *) "RQVN",       (const u8 *) "RSVN",       hitagi_command_RQVN        },
	{ (const u8 *) "RQSW",       (const u8 *) "RSSW",       hitagi_command_RQSW        },
	{ (const u8 *) "RQSN",       (const u8 *) "RSSN",       hitagi_command_RQSN        },
	{ (const u8 *) "RQFI",       (const u8 *) "RSFI",       hitagi_command_RQFI        },
	{ (const u8 *) "READ_OTP",   (const u8 *) "READ_OTP",   hitagi_command_READ_OTP    },
	{ (const u8 *) "RESTART",    (const u8 *) NULL,         hitagi_command_RESTART     },
	{ (const u8 *) "POWER_DOWN", (const u8 *) NULL,         hitagi_command_POWER_DOWN  },
#endif
};

const u8 hitagi_cmd_tbl_size = sizeof(hitagi_cmd_tbl) / sizeof(hitagi_cmd_tbl[0]);

static void hitagi_command_ADDR(const u8 *answer_str, const u8 *data_ptr, const u8 *buffer_next_byte) {
	u32 addr;
	u8 response[MAX_RESP_DATA_SIZE];

	UNUSED(answer_str);
	UNUSED(buffer_next_byte);

	addr = util_hexasc_to_u32(&data_ptr[0], CMD_32_SIZE);
	received_address_ptr = (u16 *) addr;

	util_string_copy(&response[0], data_ptr);
	hitagi_send_ack(response);
}

static void hitagi_command_BIN(const u8 *answer_str, const u8 *data_ptr, const u8 *buffer_next_byte) {
	u32 i;
	u8 *rx_ptr;
	u8 *data_aligned_ptr;
	u8 nr_shift_right;
	u8 bytes_received;
	u32 bytes_received_total;
	const u8 *source_ptr;

	UNUSED(answer_str);

	source_ptr = data_ptr;
	received_packet_size = ((source_ptr[BIN_DATA_SIZE_MSB] << SHIFT_MSB) + source_ptr[BIN_DATA_SIZE_LSB]);
	source_ptr += MAX_DATA_FIELD_SIZE;

	rx_ptr = (u8 *) buffer_next_byte;
	bytes_received_total = buffer_next_byte - source_ptr;

	while (bytes_received_total < received_packet_size) {
		bytes_received = usb_rx(rx_ptr);
		bytes_received_total += bytes_received;
		rx_ptr += bytes_received;
	}

	hitagi_send_ack(NULL);

	nr_shift_right  = sizeof(u32) - (((u32) source_ptr) % sizeof(u32));
	nr_shift_right %= sizeof(u32);
	if (nr_shift_right != 0) {
		data_aligned_ptr = (u8 *) source_ptr + received_packet_size - 1;
		for (i = 0; i < received_packet_size; i++, data_aligned_ptr--) {
			data_aligned_ptr[nr_shift_right] = data_aligned_ptr[0];
		}
		source_ptr += nr_shift_right;
		buffer_next_byte += nr_shift_right;
	}

	if (erase_cmdlet == ERASE_NO) {
		data_aligned_ptr = (u8 *) received_address_ptr;
		for (i = 0; i < received_packet_size; ++i) {
			*data_aligned_ptr++ = *source_ptr++;
		}
	} else {
		flash_unlock((volatile u16 *) received_address_ptr);

		if (flash_geometry((volatile u16 *) received_address_ptr) == RESULT_OK) {
			flash_erase((volatile u16 *) received_address_ptr);
		}

		if (erase_cmdlet != ERASE_ONLY) {
			if (erase_cmdlet == ERASE_WRITE_BLOCK) {
				flash_write_block(
					(volatile u16 *) received_address_ptr,
					(volatile u16 *) source_ptr,
					received_packet_size
				);
			} else if (erase_cmdlet == ERASE_WRITE_BUFFER) {
				flash_write_buffer(
					(volatile u16 *) received_address_ptr,
					(const u16 *) source_ptr,
					received_packet_size
				);
			} else {
				return;
			}
		}
	}

	received_address_ptr += (received_packet_size / 2);
}

static void hitagi_command_ERASE(const u8 *answer_str, const u8 *data_ptr, const u8 *buffer_next_byte) {
	u8 response[MAX_READ_RESPONSE_SIZE];

	UNUSED(answer_str);
	UNUSED(data_ptr);
	UNUSED(buffer_next_byte);

	erase_cmdlet += 1;
	util_u16_to_hexasc(erase_cmdlet, response);

	hitagi_send_ack(response);
}

static void hitagi_command_READ(const u8 *answer_str, const u8 *data_ptr, const u8 *buffer_next_byte) {
	u8 csum;
	u16 size;
	u32 start_addr;
	u8 response[MAX_READ_RESPONSE_SIZE];
	u8 *data_start_ptr;
	u8 *data_end_ptr;
	u8 *response_ptr;

	UNUSED(buffer_next_byte);

	csum = 0;
	response_ptr = &response[0];

	start_addr = util_hexasc_to_u32(&data_ptr[0], CMD_32_SIZE);
	size = util_hexasc_to_u32(&data_ptr[CMD_32_SIZE + 1], CMD_16_SIZE);

	data_start_ptr = (u8 *) start_addr;
	data_end_ptr = data_start_ptr + size;

	if (size < 0x10) {
		hitagi_send_error(ERR_DATA_INVALID);
		return;
	}

	*((u16 *) response_ptr) = size;
	response_ptr += 2;
	csum += (size >> 8) & 0xFF;
	csum += (size >> 0) & 0xFF;

	while (data_start_ptr < data_end_ptr) {
		u8 byte_data = *(data_start_ptr++);
		csum += byte_data;
		*(response_ptr++) = byte_data;
		watchdog_service();
	}

	*response_ptr = csum;

	hitagi_send_bin_packet(answer_str, response, size + 2 + 1);
}

static void hitagi_command_RQHW(const u8 *answer_str, const u8 *data_ptr, const u8 *buffer_next_byte) {
	u8 i;
	u8 *response_ptr;
	u16 bootloader_version;
	u8 response[MAX_READ_RESPONSE_SIZE];

	UNUSED(data_ptr);
	UNUSED(buffer_next_byte);

	for (i = 0; i < 8 * 2; ++i) {
		response[i] = 0x30;
	}

	response_ptr = &response[12];
	bootloader_version = hitagi_platform_bootloader_version();
	util_u16_to_hexasc(bootloader_version, response_ptr);

	hitagi_send_packet(answer_str, response);
}

static void hitagi_command_RQVN(const u8 *answer_str, const u8 *data_ptr, const u8 *buffer_next_byte) {
	u8 i;
	u8 *response_ptr;
	u16 bootloader_version;
	u8 response[MAX_READ_RESPONSE_SIZE];

	UNUSED(data_ptr);
	UNUSED(buffer_next_byte);

	for (i = 0; i < ((8 * 2) * 2) + 1; ++i) {
		response[i] = 0x30;
	}

	bootloader_version = hitagi_platform_bootloader_version();

	response_ptr = &response[12];
	util_u16_to_hexasc(bootloader_version, response_ptr);
	response[16] = *com_str;

	response_ptr = &response[29];
	util_u16_to_hexasc(bootloader_version, response_ptr);

	hitagi_send_packet(answer_str, response);
}

static void hitagi_command_RQRC(const u8 *answer_str, const u8 *data_ptr, const u8 *buffer_next_byte) {
	u16 csum;
	u8 *data_start_ptr;
	u8 *data_end_ptr;
	u8 response[MAX_RESP_DATA_SIZE];
	u32 start_addr;
	u32 end_addr;

	UNUSED(buffer_next_byte);

	csum = 0;
	start_addr = util_hexasc_to_u32(&data_ptr[0], CMD_32_SIZE);
	end_addr = util_hexasc_to_u32(&data_ptr[CMD_32_SIZE + 1], CMD_32_SIZE);

	if ((end_addr - start_addr) < 1) {
		hitagi_send_error(ERR_DATA_INVALID);
		return;
	}

	data_start_ptr = (u8 *) start_addr;
	data_end_ptr = (u8 *) end_addr;
	while (data_start_ptr <= data_end_ptr) {
		csum += *data_start_ptr;
		data_start_ptr++;
		watchdog_service();
	}

	util_u16_to_hexasc(csum, response);
	hitagi_send_packet(answer_str, response);
}

static void hitagi_command_RQSW(const u8 *answer_str, const u8 *data_ptr, const u8 *buffer_next_byte) {
	UNUSED(data_ptr);
	UNUSED(buffer_next_byte);

	hitagi_send_packet(answer_str, rqsw_response);
}

static void hitagi_command_RQSN(const u8 *answer_str, const u8 *data_ptr, const u8 *buffer_next_byte) {
	u8 response[MAX_READ_RESPONSE_SIZE];

	UNUSED(data_ptr);
	UNUSED(buffer_next_byte);

	hitagi_platform_serial_number(response);
	response[28] = NUL;

	hitagi_send_packet(answer_str, response);
}

static void hitagi_command_RQFI(const u8 *answer_str, const u8 *data_ptr, const u8 *buffer_next_byte) {
	u32 flash_part_id;
	u8 response[MAX_READ_RESPONSE_SIZE];

	UNUSED(data_ptr);
	UNUSED(buffer_next_byte);

	flash_part_id = flash_get_part_id(FLASH_START_ADDRESS);
	util_u32_to_hexasc(flash_part_id, response);

	hitagi_send_packet(answer_str, response);
}

static void hitagi_command_READ_OTP(const u8 *answer_str, const u8 *data_ptr, const u8 *buffer_next_byte) {
	u16 i;
	u16 size;
	u8 otp_reg_buffer[FLASH_MAX_OTP_SIZE];
	u8 *response_ptr;
	u8 response[MAX_READ_RESPONSE_SIZE];

	UNUSED(data_ptr);
	UNUSED(buffer_next_byte);

	response_ptr = &response[0];
	flash_get_otp_zone(FLASH_START_ADDRESS, otp_reg_buffer, &size);

	for (i = 0; i < size; ++i) {
		util_u8_to_hexasc(otp_reg_buffer[i], response_ptr);
		response_ptr += 2;
	}

	hitagi_send_packet(answer_str, response);
}

static void hitagi_command_RESTART(const u8 *answer_str, const u8 *data_ptr, const u8 *buffer_next_byte) {
	UNUSED(answer_str);
	UNUSED(data_ptr);
	UNUSED(buffer_next_byte);

	hitagi_send_ack(NULL);
	nop(1024 * 1024);
	watchdog_reboot();
}

static void hitagi_command_POWER_DOWN(const u8 *answer_str, const u8 *data_ptr, const u8 *buffer_next_byte) {
	UNUSED(answer_str);
	UNUSED(data_ptr);
	UNUSED(buffer_next_byte);

	hitagi_send_ack(NULL);
	nop(1024 * 1024);
	watchdog_shutdown();
}

void hitagi_send_ack(const u8 *data) {
	/* ACK responce data. */
	u8 response[MAX_ACK_RESPONSE_SIZE];
	u8 *response_ptr;
	u8 *command_ptr;
	u8 *end_ptr;

	response_ptr = response;
	command_ptr = rx_command;
	end_ptr = &(response_ptr[MAX_ACK_RESPONSE_SIZE - 1]);

	/* Copy last command to ACK response. */
	while ((*command_ptr != NUL) && (response_ptr != end_ptr)) {
		*(response_ptr++) = *(command_ptr++);
	}

	/* If there is data, add a comma, then the data. */
	if (data) {
		*(response_ptr++) = com_str[0];

		/* Copy data into response. */
		while ((*data != NUL) && (response_ptr != end_ptr)) {
			*(response_ptr++) = *(data++);
		}
	}

	/* Terminate data. */
	*response_ptr = NUL;

	hitagi_send_packet((const u8 *) "ACK", response);
}

void hitagi_send_packet(const u8 *cmd, const u8 *data) {
	hitagi_lte_send_packet_aux(cmd, data, 0);
}

void hitagi_send_bin_packet(const u8 *cmd, const u8 *data, u16 bin_size) {
	hitagi_lte_send_packet_aux(cmd, data, bin_size);
}

static void hitagi_lte_send_packet_aux(const u8 *cmd, const u8 *data, u16 bin_size) {
	u16 i;
	u16 j;
	u16 size;
	u8 *cmd_ptr;
	u8 *response;
	u8 empty_packet[] = { 0x00, 0x00 };

	i = 0;
	j = 0;
	size = bin_size;

	/* Save the original command on argument and set tx_data buffer as a response buffer. */
	cmd_ptr  = (u8 *) cmd;
	response = tx_data;

	/* Attach the starting control/transmition character first. */
	response[i++] = STX;

	/* Place command to the first answer section. */
	while ((*cmd_ptr != NUL) && (i < (USB_MAX_TX_DATA_SIZE - 16))) {
		response[i++] = *(cmd_ptr++);
	}

	/* Check if any data is present. */
	if (bin_size > 0) {
		/* Place separator character because data is present. */
		response[i++] = RS;

		/* Place data to the answer. */
		while ((size > 0) && (i < (USB_MAX_TX_DATA_SIZE - 16))) {
			response[i++] = *(data++);
			size--;
		}
	} else {
		if (data != NULL) {
			/* Place separator character because data is present. */
			response[i++] = RS;

			/* Place data to the answer. */
			while ((*data != NUL) && (i < (USB_MAX_TX_DATA_SIZE - 16))) {
				response[i++] = *(data++);
			}
		}
	}

	/* Place the ending control/transmition character and terminate string. */
	response[i++] = ETX;
	response[i] = NUL;

	/* Send full-fledged answer! */
	while (i > 0) {
		/* Big portion of Tx data. */
		if (i >= USB_MAX_PACKET_SIZE) {
			/* Send out Tx data and wait for it. */
			while (usb_tx(&(response[j]), USB_MAX_PACKET_SIZE) != RESULT_OK);

			/* Increment to the next data portion and decrement the data size. */
			j += USB_MAX_PACKET_SIZE;
			i -= USB_MAX_PACKET_SIZE;

			/* If we're sending data that is % USB_MAX_PACKET_SIZE, we must send an empty USB data packet. */
			if (i == 0) {
				while (usb_tx(&empty_packet[0], 0) != RESULT_OK);
			}
		} else {
			/* Small portion of Tx data. */
			while (usb_tx(&(response[j]), i) != RESULT_OK);

			i = 0;
		}
	}
}
