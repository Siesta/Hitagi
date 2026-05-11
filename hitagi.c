/*
 * About:
 *   Hitagi main module with Motorola Flash Protocol implementation.
 *
 * Author:
 *   EXL, Motorola Inc.
 *
 * License:
 *   MIT
 *
 * Commands:
 *   ADDR        |.ADDR.10000000XX.       |  # Set address for BIN command, XX is checksum.
 *   BIN         |.BIN.DATAXX.            |  # Upload binary to address (IRAM, RAM, Flash for flashing), XX is checksum.
 *   ERASE       |.ERASE.                 |  # Activate read and write mode. See below for more details.
 *   READ        |.READ.10000000,0200.    |  # Read data from address on size.
 *   RQHW        |.RQHW.                  |  # Request hardware info data, bootloader version.
 *   RQRC        |.RQRC.10000000,10000600.|  # Calculate checksum of addresses range.
 *   RQVN        |.RQVN.                  |  # Request version info.
 *   RQSW        |.RQSW.                  |  # Request S/W version.
 *   RQSN        |.RQSN.                  |  # Request serial number of SoC.
 *   RQFI        |.RQFI.                  |  # Request part ID from flash memory chip.
 *   READ_OTP    |.READ_OTP.              |  # Read OTP registers data.
 *   RESTART     |.RESTART.               |  # Restart or power down device.
 *   POWER_DOWN  |.POWER_DOWN.            |  # Power off device.
 *
 * Notes:
 *   1. Flash modes can be switched by calling the method that sets the `ERASE` flag a different number of times.
 *
 *      # 0. Read-only mode. No `ERASE` flag is set.
 *
 *      # 1. Read/Write word mode for the entire flash.
 *      mfp_cmd(er, ew, 'ERASE')
 *
 *      # 2. Read/Write buffer mode for the entire flash.
 *      mfp_cmd(er, ew, 'ERASE')
 *      mfp_cmd(er, ew, 'ERASE')
 *
 *      # 3. Erase-only mode for the entire flash.
 *      mfp_cmd(er, ew, 'ERASE')
 *      mfp_cmd(er, ew, 'ERASE')
 *      mfp_cmd(er, ew, 'ERASE')
 *
 *   2. It is better if the flashed chunk size is a multiple of `0x8000` (parameter blocks) or `0x20000` (main blocks)
 *      for Intel-like and AMD-like flash chips.
 */

#include "hitagi_protocol.h"

/**
 * Functions.
 */

void util_u8_to_hexasc(u8 val, u8 *str);
void util_u16_to_hexasc(u16 val, u8 *str);
void util_u32_to_hexasc(u32 val, u8 *str);
u32 util_hexasc_to_u32(const u8 *str, u8 size);
void util_string_copy(u8 *dst, const u8 *src);
static int util_string_equal(const u8 *str1_ptr, const u8 *str2_ptr);
static int util_map_cmd(const HITAGI_CMD_TABLE_T *table_ptr, u8 table_size, const u8 *cmd);

static void hitagi_commands(const u8 *cmd, const u8 *data, const u8 *next);
void hitagi_send_error(u8 error_code);
static void hitagi_read_packets(void);

/**
 * Constants and command table.
 */

static const u8 err_str[]  = "ERR";
static const u8 bin_str[]  = "BIN";

/**
 * Globals and Rx/Tx buffers.
 */

u16 *received_address_ptr;
u16  received_packet_size;

u8 rx_command[MAX_COMMAND_STR_SIZE];

#if !defined(FTR_COMPACT)
static u8 rx_data_storage[USB_MAX_RX_DATA_SIZE];
static u8 tx_data_storage[USB_MAX_TX_DATA_SIZE];
u8 *rx_data = rx_data_storage;
u8 *tx_data = tx_data_storage;
#else
u8 *rx_data = (u8 *) 0x03FD0000 + 0x10000;
u8 *tx_data = (u8 *) 0x03FD0000 + 0x10000 + USB_MAX_RX_DATA_SIZE;
#endif

HITAGI_CMDLET_ERASE_T erase_cmdlet;

/**
 * NOP function.
 */

void nop(u32 nop_count) {
	u32 i;
	for (i = 0; i < nop_count; ++i) {
		asm volatile ("nop");
	}
}

/**
 * Util functions.
 */

void util_u8_to_hexasc(u8 val, u8 *str) {
	u8 i;
	u8 digit;

	for (i = 0; i < 2; ++i) {
		digit = (val >> 4) & 0x0F;
		val <<= 4;
		*str++ = (digit > 9) ? (digit + '7') : (digit + '0');
	}

	*str = NUL;
}

void util_u16_to_hexasc(u16 val, u8 *str) {
	u8 i;
	u8 digit;

	for (i = 0; i < 4; ++i) {
		digit = (val >> 12) & 0x0F;
		val <<= 4;
		*str++ = (digit > 9) ? (digit + '7') : (digit + '0');
	}

	*str = NUL;
}

void util_u32_to_hexasc(u32 val, u8 *str) {
	u8 i;
	u8 digit;

	for (i = 0; i < 8; ++i) {
		digit = (val >> 28) & 0x0F;
		val <<= 4;
		*str++ = (digit > 9) ? (digit + '7') : (digit + '0');
	}

	*str = NUL;
}

u32 util_hexasc_to_u32(const u8 *str, u8 size) {
	u8 digit;
	u32 val = 0;

	while (size--) {
		digit = *str++;
		val <<= 4; /* Shift previous digit over. */
		val += (digit >= 'A') ? (digit - '7') : (digit - '0');
	}

	return val;
}

void util_string_copy(u8 *dst, const u8 *src) {
	/* The do-while loop will copy the NUL terminator! */
	do {
		*dst++ = *src;
	} while (*src++);
}

static int util_string_equal(const u8 *str1_ptr, const u8 *str2_ptr) {
	int match = 0;

	while (*str1_ptr && *str2_ptr && (*str1_ptr == *str2_ptr)) {
		str1_ptr++;
		str2_ptr++;
	}

	if (*str1_ptr == *str2_ptr) {
		match = 1;
	}

	return match;
}

static int util_map_cmd(const HITAGI_CMD_TABLE_T *table_ptr, u8 table_size, const u8 *cmd) {
	u8 i;
	for (i = 0; i < table_size; ++i) {
		if (util_string_equal(table_ptr[i].cmd, cmd)) {
			return i;
		}
	}
	return -1;
}

/**
 * Command dispatcher.
 */

static void hitagi_commands(const u8 *cmd, const u8 *data, const u8 *next) {
	int idx = util_map_cmd(&hitagi_cmd_tbl[0], hitagi_cmd_tbl_size, cmd);

	if (idx >= 0 && hitagi_cmd_tbl[idx].cmd_func) {
		hitagi_cmd_tbl[idx].cmd_func(hitagi_cmd_tbl[idx].answer_str, data, next);
	} else {
		hitagi_send_error(ERR_UNKNOWN_COMMAND);
	}
}

void hitagi_send_error(u8 error_code) {
	u8 error_code_str[2];

	error_code_str[0] = error_code;
	error_code_str[1] = NUL;

	hitagi_send_packet(err_str, error_code_str);
}

static void hitagi_read_packets(void) {
	u8 i;
	u8 bytes_received;
	u8 previous_command_offset;
	u16 accumulated_bytes_received;
	u16 data_bytes_to_read;

	u8 data_array[USB_DATA_ARRAY_SIZE];

	u8 *input_ptr;
	u8 *command_ptr;
	u8 *current_ptr;
	u8 *buffer_next_byte;
	u8 *data_ptr;

	bytes_received = 0;
	previous_command_offset = 0;
	data_bytes_to_read = 0;

	input_ptr = data_array;
	command_ptr = rx_command;
	buffer_next_byte = NULL;

	watchdog_service();

	/* Forever! */
	while ("MotoFan.Ru is rock!") {
		/* Check if there is data coming in on EP1, add to any data left over from previous command. */
		bytes_received = usb_rx((u8 *) (input_ptr + previous_command_offset));

		if (bytes_received != 0) {
			/* Add the previous data to the count. */
			bytes_received += previous_command_offset;

			/* Throw out all data read until an STX is found. */
			current_ptr = input_ptr;
			while ((*(current_ptr++) != STX) && (bytes_received != 0)) {
				bytes_received--;
			}

			/* STX is present, process the rest of the message. */
			if (bytes_received != 0) {
				/* Decrement to throw out STX. */
				bytes_received--;

				/* Copy the remainder of the first read until RS or ETX is found. */
				while ((*current_ptr != ETX) && (*current_ptr != RS)) {
					*(command_ptr++) = *(current_ptr++);

					/* If out of data, then read more. */
					if (bytes_received-- == 0) {
						bytes_received = usb_rx(input_ptr);
						current_ptr = input_ptr;
					}
				}

				/* Done reading in command, terminate it! */
				*command_ptr = NUL;

				/* Check for separator and additional payload data. */
				if (*current_ptr == RS) {
					/* Set up RX data buffer. */
					data_ptr = rx_data;
					accumulated_bytes_received = 0;

					/* Skip RS and go into data. */
					current_ptr++;
					bytes_received--;

					/* Save all read bytes into data buffer. */
					while (bytes_received != 0) {
						bytes_received--;
						*(data_ptr++) = *(current_ptr++);
						accumulated_bytes_received++;
					}

					/* If this is a BIN command. */
					if (util_string_equal(bin_str, rx_command)) {
						/* Retrieve the minimum bytes required so we can get the size of the BIN data. */
						while (accumulated_bytes_received < MAX_DATA_FIELD_SIZE) {
							bytes_received += usb_rx(data_ptr);
							accumulated_bytes_received += bytes_received;
							data_ptr += bytes_received;
						}

						/* Determine the numbers of bytes to read. */
						data_bytes_to_read = ((rx_data[BIN_DATA_SIZE_MSB] << SHIFT_MSB) + (rx_data[BIN_DATA_SIZE_LSB]));

						/* Check for a valid data packet size. */
						if (
							(data_bytes_to_read < MIN_BIN_PACKET_SIZE) ||
							(data_bytes_to_read > MAX_BIN_PACKET_SIZE) ||
							(data_bytes_to_read % EVEN_NUMBER)
						) {
							hitagi_send_error(ERR_INVALID_PACKET_SIZE);
						} else {
							/* Adjust size of bytes to read, one byte for checksum, one byte for ETX. */
							/* And MAX_DATA_FIELD_SIZE for data count. */

							data_bytes_to_read += (MAX_DATA_FIELD_SIZE + 2);

							/* The next "free" location in the buffer is... */
							buffer_next_byte = data_ptr;

							/* Reset the data_ptr to the start of the buffer. */
							data_ptr = rx_data;
						}
					} else {
						/* Is not a BIN command but with DATA field also. */
						previous_command_offset = 0;

						data_ptr = rx_data;

						/* Scan for end of data. */
						while ((accumulated_bytes_received != 0) && (*data_ptr != ETX)) {
							data_ptr++;
							accumulated_bytes_received--;
						}

						/* Check if ETX was found. */
						if (accumulated_bytes_received != 0) {
							/* If ETX found set offset for next time around loop and skip ETX. */
							previous_command_offset = accumulated_bytes_received - 1;
							data_ptr++;
						} else {
							/* ETX not found yet, read more data. */
							while (bytes_received == 0) {
								watchdog_service();
								bytes_received = usb_rx(data_ptr);
							}
							while (*data_ptr != ETX) {
								data_ptr++;
								/* If out of data, read more again. */
								bytes_received--;
								if (!bytes_received) {
									bytes_received = usb_rx(data_ptr);
								}
								watchdog_service();
							}

							/* Set number of characters for next go-around, minus ETX. */
							if (bytes_received != 0) {
								previous_command_offset = bytes_received - 1;
							}

							/* Skip ETX character. */
							data_ptr++;
						}

						/* Copy any extra data back out for next command. */
						input_ptr = data_array;
						for (i = 0; i < previous_command_offset; ++i) {
							*(input_ptr++) = *(data_ptr++);
						}

						/* Add NULL-terminator to data and reset data pointer to beginning of rx_data array. */
						*data_ptr = NUL;
						data_ptr = rx_data;
					}
				} else {
					data_ptr = NULL;
				}

				hitagi_commands(rx_command, data_ptr, buffer_next_byte);

				/* End of command data, reset all pointers. */
				command_ptr = rx_command;
				data_ptr = NULL;
				input_ptr = data_array;
			}
		}

		watchdog_service();
	}
}

void hitagi_start(void) {
	usb_init();
	hitagi_platform_init();

	hitagi_read_packets();
}
