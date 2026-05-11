/*
 * About:
 *   ArgonLV platform support for Hitagi RAMDLD.
 *
 * License:
 *   MIT
 */

#include "hitagi_platform.h"

#define USB_RX_X_BUFFER                 1
#define USB_RX_Y_BUFFER                 2
#define ARGONLV_BOOTLOADER_VERSION_ADDR ((volatile u8 *) 0xA001001E)

static void usb_copy_block(const u8 *src, volatile u32 *dst, u16 len);
static void usb_read_block(volatile u32 *src, u8 *dst, u16 len);
static void usb_w1c(volatile u32 *reg, u32 mask);
static void usb_ep_set_ttl(u8 ep, u32 len);
static void usb_prepare_mfp_tx(void);
static void argon_pu_cache_setup(void);

static u8 usb_tx_first = 1;
static u8 usb_rx_buffer = USB_RX_X_BUFFER;
static u32 usb_rx_total_bytes;

static const u32 usb_mfp_tx_init[4] = {
	USB_EP_DESC_MAXPKTSIZ_64 | USB_EP_DESC_FORMAT_BULK,
	(ARGONLV_USB_EP2_IN_XBUF << 16) | ARGONLV_USB_EP2_IN_XBUF,
	0,
	USB_EP_DESC_BUFSIZE_64 | USB_MAX_PACKET_SIZE
};

static void usb_copy_block(const u8 *src, volatile u32 *dst, u16 len) {
	u16 i;
	u16 word_count;
	u8 byte_count;
	u32 data;

	word_count = len / sizeof(u32);
	byte_count = len % sizeof(u32);

	for (i = 0; i < word_count; ++i) {
		data  = ((u32) src[0]) << 0;
		data |= ((u32) src[1]) << 8;
		data |= ((u32) src[2]) << 16;
		data |= ((u32) src[3]) << 24;
		*dst++ = data;
		src += sizeof(u32);
	}

	if (byte_count != 0) {
		data = 0;
		for (i = 0; i < byte_count; ++i) {
			data |= ((u32) *src++) << (8 * i);
		}
		*dst = data;
	}
}

static void usb_read_block(volatile u32 *src, u8 *dst, u16 len) {
	u16 i;
	u8 j;
	u16 word_count;
	u8 byte_count;
	u32 data;

	word_count = len / sizeof(u32);
	byte_count = len % sizeof(u32);

	for (i = 0; i < word_count; ++i) {
		data = *src++;
		for (j = 0; j < sizeof(u32); ++j) {
			*dst++ = (u8) (data >> (8 * j));
		}
	}

	if (byte_count != 0) {
		data = *src;
		for (j = 0; j < byte_count; ++j) {
			*dst++ = (u8) (data >> (8 * j));
		}
	}
}

static void usb_w1c(volatile u32 *reg, u32 mask) {
	if ((*reg & mask) == mask) {
		*reg = mask;
	}
}

static void usb_ep_set_ttl(u8 ep, u32 len) {
	ARGONLV_USB_EP_DESC[ep].dword3 =
		(ARGONLV_USB_EP_DESC[ep].dword3 & ~USB_EP_DESC_TTLBTECNT_MASK) |
		(len & USB_EP_DESC_TTLBTECNT_MASK);
}

static void usb_prepare_mfp_tx(void) {
	ARGONLV_USB_EP_DESC[USB_MFP_TX_EP].dword0 = usb_mfp_tx_init[0];
	ARGONLV_USB_EP_DESC[USB_MFP_TX_EP].dword1 = usb_mfp_tx_init[1];
	ARGONLV_USB_EP_DESC[USB_MFP_TX_EP].dword2 = usb_mfp_tx_init[2];
	ARGONLV_USB_EP_DESC[USB_MFP_TX_EP].dword3 = usb_mfp_tx_init[3];

	ARGONLV_USB_FUNC.ep_en_reg |= USB_MFP_TX_MASK;
	ARGONLV_USB_FUNC.ep_done_en_reg |= USB_MFP_TX_MASK;
	ARGONLV_USB_FUNC.xy_int_en_reg |= USB_MFP_TX_MASK;

	usb_w1c(&ARGONLV_USB_FUNC.ep_done_status_reg, USB_MFP_TX_MASK);
	usb_w1c(&ARGONLV_USB_FUNC.xbuff_int_status_reg, USB_MFP_TX_MASK);
	usb_w1c(&ARGONLV_USB_FUNC.ybuff_int_status_reg, USB_MFP_TX_MASK);
	usb_w1c(&ARGONLV_USB_FUNC.xfilled_status_reg, USB_MFP_TX_MASK);
	usb_w1c(&ARGONLV_USB_FUNC.yfilled_status_reg, USB_MFP_TX_MASK);

	usb_tx_first = 1;
}

int argonlv_usb_tx_fast(const u8 *src, u8 len) {
	u32 ep_done;

	ep_done = ARGONLV_USB_FUNC.ep_done_status_reg & USB_MFP_TX_MASK;
	if ((ep_done == 0) && (usb_tx_first == 0)) {
		return RESULT_FAIL;
	}

	ARGONLV_USB_FUNC.frame_num_reg = USB_MFP_TX_MASK;
	if (ep_done != 0) {
		usb_w1c(&ARGONLV_USB_FUNC.ep_done_status_reg, USB_MFP_TX_MASK);
	}

	usb_ep_set_ttl(USB_MFP_TX_EP, len);
	usb_copy_block(src, ARGONLV_USB_EP_XBUF(USB_MFP_TX_EP), len);

	ARGONLV_USB_FUNC.xfilled_status_reg = USB_MFP_TX_MASK;
	ARGONLV_USB_FUNC.ep_ready_reg |= USB_MFP_TX_MASK;
	usb_tx_first = 0;

	return RESULT_OK;
}

int usb_tx(const u8 *src, u8 len) {
	return argonlv_usb_tx_fast(src, len);
}

u8 usb_rx(u8 *dst) {
	u32 rx_bytes;
	u32 remaining;
	u32 ep_done;
	u32 xfilled;
	u32 yfilled;
	volatile u32 *src;

	rx_bytes = 0;
	ep_done = ARGONLV_USB_FUNC.ep_done_status_reg & USB_MFP_RX_MASK;
	xfilled = ARGONLV_USB_FUNC.xfilled_status_reg & USB_MFP_RX_MASK;
	yfilled = ARGONLV_USB_FUNC.yfilled_status_reg & USB_MFP_RX_MASK;

	if (((ep_done != 0) && (xfilled != 0)) ||
	    ((ep_done != 0) && (yfilled != 0)) ||
	    ((xfilled != 0) && (yfilled != 0))) {
		if (usb_rx_buffer == USB_RX_X_BUFFER) {
			src = ARGONLV_USB_EP_XBUF(USB_MFP_RX_EP);
		} else {
			src = ARGONLV_USB_EP_YBUF(USB_MFP_RX_EP);
		}

		remaining = ARGONLV_USB_EP_DESC[USB_MFP_RX_EP].dword3 & USB_EP_DESC_TTLBTECNT_MASK;
		if (remaining < USB_MFP_RX_TOTAL_BYTES) {
			rx_bytes = USB_MFP_RX_TOTAL_BYTES - remaining - usb_rx_total_bytes;
			if (rx_bytes > USB_MAX_PACKET_SIZE) {
				rx_bytes = USB_MAX_PACKET_SIZE;
			}

			usb_read_block(src, dst, (u16) rx_bytes);
			usb_rx_total_bytes += rx_bytes;
		}

		if (usb_rx_buffer == USB_RX_X_BUFFER) {
			usb_w1c(&ARGONLV_USB_FUNC.xfilled_status_reg, USB_MFP_RX_MASK);
			usb_rx_buffer = USB_RX_Y_BUFFER;
		} else {
			usb_w1c(&ARGONLV_USB_FUNC.yfilled_status_reg, USB_MFP_RX_MASK);
			usb_rx_buffer = USB_RX_X_BUFFER;
		}
	}

	ep_done = ARGONLV_USB_FUNC.ep_done_status_reg & USB_MFP_RX_MASK;
	xfilled = ARGONLV_USB_FUNC.xfilled_status_reg & USB_MFP_RX_MASK;
	yfilled = ARGONLV_USB_FUNC.yfilled_status_reg & USB_MFP_RX_MASK;

	if ((ep_done != 0) && (xfilled == 0) && (yfilled == 0)) {
		ARGONLV_USB_FUNC.frame_num_reg = USB_MFP_RX_MASK;
		usb_rx_buffer = USB_RX_X_BUFFER;
		usb_w1c(&ARGONLV_USB_FUNC.ep_done_status_reg, USB_MFP_RX_MASK);
		usb_rx_total_bytes = 0;
		usb_ep_set_ttl(USB_MFP_RX_EP, USB_MFP_RX_TOTAL_BYTES);
		ARGONLV_USB_FUNC.ep_ready_reg |= USB_MFP_RX_MASK;
	}

	return (u8) rx_bytes;
}

int usb_init(void) {
	watchdog_service();
	usb_prepare_mfp_tx();

	return RESULT_OK;
}

int watchdog_reboot(void) {
	ARGONLV_WATCHDOG1.control &= ~(ARGONLV_NOT_SW_RESET);

	while("MotoFan.Ru is cool!");

	return RESULT_OK;
}

int watchdog_shutdown(void) {
	ARGONLV_WATCHDOG1.control &= ~(ARGONLV_WD_NOT_ASSERTED);

	while("MotoFan.Ru is cool!");

	return RESULT_OK;
}

int watchdog_service(void) {
	ARGONLV_WATCHDOG1.service = 0x5555;
	ARGONLV_WATCHDOG1.service = 0xAAAA;
	ARGONLV_WATCHDOG2.service = 0x5555;
	ARGONLV_WATCHDOG2.service = 0xAAAA;

	return RESULT_OK;
}

int hitagi_platform_init(void) {
	erase_cmdlet = ERASE_NO;
	watchdog_service();

	return RESULT_OK;
}

u16 hitagi_platform_bootloader_version(void) {
	return (((u16) ARGONLV_BOOTLOADER_VERSION_ADDR[0]) << 8) |
	       ((u16) ARGONLV_BOOTLOADER_VERSION_ADDR[1]);
}

void hitagi_platform_serial_number(u8 *response_ptr) {
	u8 uid[ARGONLV_UID_SIZE];
	u8 i;

	uid[0] = (u8) ARGONLV_IC_ID_REG.uid6;
	uid[1] = (u8) ARGONLV_IC_ID_REG.uid5;
	uid[2] = (u8) ARGONLV_IC_ID_REG.uid4;
	uid[3] = (u8) ARGONLV_IC_ID_REG.uid3;
	uid[4] = (u8) ARGONLV_IC_ID_REG.uid2;
	uid[5] = (u8) ARGONLV_IC_ID_REG.uid1;
	uid[6] = (u8) ARGONLV_IC_ID_REG.uid0;

	for (i = 0; i < ARGONLV_UID_SIZE; ++i) {
		util_u8_to_hexasc(uid[i], response_ptr);
		response_ptr += 2;
	}
}

static void __attribute__((naked, noinline, used, section(".startup.cache"))) argon_pu_cache_setup(void) {
	asm volatile (
		"push {lr}\n"
		"ldr r0, =0x30000000\n"
		"ldr r1, =0x0003001b\n"
		"str r1, [r0, #0x104]\n"
		"mov r1, #0xff\n"
		"str r1, [r0, #0x77c]\n"
		"1:\n"
		"ldr r1, [r0, #0x77c]\n"
		"cmp r1, #0\n"
		"bne 1b\n"
		"mov r0, #0\n"
		"mcr p15, 0, r0, c8, c7, 0\n"
		"mov r0, #0x200\n"
		"mov r1, #0x4d\n"
		"mov r2, #0\n"
		"ldr r3, =0x80000000\n"
		"bl 5f\n"
		"ldr r0, =0x1fff0200\n"
		"ldr r1, =0x1fff0007\n"
		"mov r2, #0\n"
		"ldr r3, =0x80000001\n"
		"bl 5f\n"
		"ldr r0, =0x30000200\n"
		"ldr r1, =0x30000007\n"
		"mov r2, #0x10\n"
		"ldr r3, =0x80000002\n"
		"bl 5f\n"
		"ldr r0, =0xb8020200\n"
		"ldr r1, =0xb8020217\n"
		"mov r2, #0x0c\n"
		"ldr r3, =0x80000003\n"
		"bl 5f\n"
		"ldr r0, =0xb8030200\n"
		"ldr r1, =0xb8030217\n"
		"mov r2, #0x0c\n"
		"ldr r3, =0x80000004\n"
		"bl 5f\n"
		"ldr r0, =0x43f00200\n"
		"ldr r1, =0x43f00007\n"
		"mov r2, #0x16\n"
		"ldr r3, =0x80000005\n"
		"bl 5f\n"
		"ldr r0, =0x53f00200\n"
		"ldr r1, =0x53f00007\n"
		"mov r2, #0x16\n"
		"ldr r3, =0x80000006\n"
		"bl 5f\n"
		"ldr r0, =0x50000200\n"
		"ldr r1, =0x50000007\n"
		"mov r2, #0x16\n"
		"ldr r3, =0x80000007\n"
		"bl 5f\n"
		"ldr r0, =0xb8000200\n"
		"ldr r1, =0xb8000207\n"
		"mov r2, #0\n"
		"ldr r3, =0x80000008\n"
		"bl 5f\n"
		"ldr r4, =0x80000000\n"
		"mov r5, #0\n"
		"2:\n"
		"orr r0, r4, #0x200\n"
		"orr r1, r4, #0x47\n"
		"mov r2, #0\n"
		"and r3, r5, #0x3f\n"
		"bl 5f\n"
		"add r5, r5, #1\n"
		"add r4, r4, #0x01000000\n"
		"cmp r5, #0x39\n"
		"bne 2b\n"
		"bl 3f\n"
		"ldr r0, =0x55555555\n"
		"mcr p15, 0, r0, c3, c0, 0\n"
		"mrc p15, 0, r0, c1, c0, 0\n"
		"ldr r1, =0x00850071\n"
		"orr r0, r0, r1\n"
		"mcr p15, 0, r0, c1, c0, 0\n"
		"bl 4f\n"
		"ldr r0, =0x30000000\n"
		"mov r2, #0\n"
		"str r2, [r0, #0x100]\n"
		"str r2, [r0, #0x730]\n"
		"mov r1, #0xff\n"
		"str r1, [r0, #0x900]\n"
		"str r1, [r0, #0x904]\n"
		"bl 3f\n"
		"pop {pc}\n"
		"3:\n"
		"ldr r0, =0x30000000\n"
		"mov r1, #1\n"
		"str r1, [r0, #0x100]\n"
		"bx lr\n"
		"4:\n"
		"mov r1, #0\n"
		"mov r2, #0\n"
		"mov r3, #0\n"
		"mov r4, #0\n"
		"mov r5, #0\n"
		"mov r6, #0\n"
		"mov r7, #0\n"
		"mov r8, #0\n"
		"ldr r0, =0xb8020000\n"
		"ldr r9, =0xb8040000\n"
		"6:\n"
		"stmia r0!, {r1-r8}\n"
		"cmp r0, r9\n"
		"bne 6b\n"
		"bx lr\n"
		"5:\n"
		"mcr p15, 5, r0, c15, c5, 2\n"
		"mcr p15, 5, r1, c15, c6, 2\n"
		"mcr p15, 5, r2, c15, c7, 2\n"
		"mcr p15, 5, r3, c15, c4, 4\n"
		"bx lr\n"
	);
}

void __attribute__((naked, section(".startup.entry"))) _start(void) {
	asm volatile (
		"mov r0, #0x40000015\n"
		"mcr p15, 0, r0, c15, c2, 4\n"
		"mrc p15, 0, r0, c1, c0, 0\n"
		"orr r0, r0, #0x00000002\n"
		"orr r0, r0, #0x00400000\n"
		"orr r0, r0, #0x00000800\n"
		"mcr p15, 0, r0, c1, c0, 0\n"
		"msr cpsr_c, #0xdf\n"
		"ldr sp, =0x81fffe00\n"
		"msr cpsr_c, #0xdb\n"
		"ldr sp, =0x81ffde00\n"
		"msr cpsr_c, #0xd7\n"
		"ldr sp, =0x81ffdd00\n"
		"msr cpsr_c, #0xd1\n"
		"ldr sp, =0x81ffba00\n"
		"msr cpsr_c, #0xd2\n"
		"ldr sp, =0x81ffca00\n"
		"msr cpsr_c, #0xd3\n"
		"ldr sp, =0x1fffffbc\n"
		"bl argon_pu_cache_setup\n"
		"mrs r0, cpsr\n"
		"bic r0, r0, #0x100\n"
		"msr cpsr_x, r0\n"
		"mrc p15, 0, r0, c1, c0, 2\n"
		"orr r0, r0, #0x00f00000\n"
		"mcr p15, 0, r0, c1, c0, 2\n"
		"mov r0, #0\n"
		"mcr p15, 0, r0, c7, c5, 4\n"
		"mov r0, #0\n"
		".word 0xeee80a10\n"
		"bl hitagi_start\n"
		"b .\n"
	);
}
