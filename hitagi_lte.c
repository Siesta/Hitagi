/*
 * About:
 *   Neptune LTE platform support for Hitagi RAMDLD.
 *
 * License:
 *   MIT
 */

#include "hitagi_platform.h"
#include "flash.h"

static void usb_copy_block(const u8 *src, u16 *dst, u8 len);
static int watchdog_init(void);

static void usb_copy_block(const u8 *src, u16 *dst, u8 len) {
	u8 i;
	u8 half_len;
	u16 data;

	half_len = len / 2;

	for (i = 0; i < half_len; ++i) {
		data = (u16) (*src++ << 8);
		*dst++ = (data | *src++);
	}

	if ((len % 2) != 0) {
		data = (u16) (*src << 8);
		*dst = data;
	}
}

int usb_tx(const u8 *src, u8 len) {
	/*
	 * (1 << 6):  EP2_int
	 * (1 << 10): XFREN
	 * (1 << 13): IEOFC
	 */
	if ((USB_INT & (1 << 6)) || !(USB_E2_CR & (1 << 10))) {
		usb_copy_block(src, USB_E2_TX_HW_BUFFER, len);

		USB_E2_CR = (USB_E2_CR & ~0x3F) | (len & 0x3F);
		USB_E2_CR |= (1 << 13);
		USB_E2_CR |= (1 << 10);

		return RESULT_OK;
	}

	return RESULT_FAIL;
}

u8 usb_rx(u8 *dst) {
	u8 i;
	u8 rx_bytes;

	rx_bytes = 0;

	if ((USB_INT & (1 << 5))) {
		if (USB_E1_CR & (1 << 13)) {
			u8 *p_dst = dst;
			u8 *p_src = (u8 *) USB_E1_RX_HW_BUFFER;

			rx_bytes = USB_E1_CR & 0x3F;

			for (i = 0; i < rx_bytes; ++i) {
				*p_dst++ = *p_src++;
			}

			USB_E1_CR |= (1 << 13);
		} else {
			USB_E1_CR |= (1 << 11);
		}
	}

	return rx_bytes;
}

int usb_init(void) {
	/*
	 * USB_MEMMAP_EP1_EP2_16BYTES = 0x0000
	 * USB_MEMMAP_EP1_EP2_32BYTES = 0x0001
	 */
	USB_CPU_CR = 0x0001;

	return RESULT_OK;
}

int watchdog_reboot(void) {
	/*
	 * PU_MAIN_SOFTWARE_RESET_PU = 0x0A
	 */
	if (RTC_PCRAM0 < 0x0A) {
		RTC_PCRAM0 = 0x0A;
	}

	/*
	 * NOT_SW_RESET = 0x0010
	 * WATCHDOG_WCR &= ~(NOT_SW_RESET)
	 */
	WATCHDOG_WCR &= ~(0x0010);

	while("MotoFan.Ru is cool!");

	return RESULT_OK;
}

int watchdog_shutdown(void) {
	/*
	 * WD_NOT_ASSERTED = 0x0020
	 * WATCHDOG_WCR &= ~(WD_NOT_ASSERTED)
	 */
	WATCHDOG_WCR &= ~(0x0020);

	while("MotoFan.Ru is cool!");

	return RESULT_OK;
}

int watchdog_service(void) {
	WATCHDOG_WSR = 0x5555;
	WATCHDOG_WSR = 0xAAAA;

	return RESULT_OK;
}

static int watchdog_init(void) {
	/*
	 * (THIRTYTWO_SEC_TIMEOUT | WD_OUTPUT_EN | WD_NOT_ASSERTED | NOT_SW_RESET | WD_ENABLE | WD_DEBUG)
	 *
	 * hex((63 << 9) | 0x0040 | 0x0020 | 0x0010 | 0x0004 | 0x0002)
	 * '0x7e76'
	 */
	WATCHDOG_WCR = 0x7E76;

	watchdog_service();

	return RESULT_OK;
}

int hitagi_platform_init(void) {
	flash_init();
	watchdog_init();

	return RESULT_OK;
}

u16 hitagi_platform_bootloader_version(void) {
	return *(FLASH_START_ADDRESS + 0x07); /* 0x0E offset (0x0E / 2) bootloader version on 0x1000000E. */
}

void hitagi_platform_serial_number(u8 *response_ptr) {
	volatile u16 *i;
	volatile u16 *start = NEPTUNE_UID_REG_ADDR;
	volatile u16 *end = (NEPTUNE_REV_REG_ADDR - 1);

	for (i = end; i >= start; --i) {
		util_u16_to_hexasc(*i, response_ptr);
		response_ptr += 4;
	}
}

void __attribute__((naked, section(".startup"))) _start(void) {
	asm volatile (
		"bl hitagi_start\n"
		"b .\n"
	);
}
