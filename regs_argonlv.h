/*
 * About:
 *   Motorola/Freescale ArgonLV register definitions used by Hitagi.
 *
 * Source:
 *   Corsica Mono R263312 product config and Argon USB bootloader headers.
 *
 * License:
 *   MIT
 */

#ifndef REGS_ARGONLV_H
#define REGS_ARGONLV_H

#include "platform.h"

/**
 * Memory map.
 */

#define ARGONLV_RAM_BASE                 0x80000000
#define ARGONLV_RAM_SIZE                 0x04000000
#define ARGONLV_FLASH_BASE               0xA0000000
#define ARGONLV_FLASH_SIZE               0x04000000

/**
 * Watchdog section.
 */

typedef struct {
	volatile u16 control;
	volatile u16 service;
	volatile u16 status;
} ARGONLV_WATCHDOG_REG_T;

#define ARGONLV_WATCHDOG1                (*(volatile ARGONLV_WATCHDOG_REG_T *) 0x53FDC000)
#define ARGONLV_WATCHDOG2                (*(volatile ARGONLV_WATCHDOG_REG_T *) 0x43F88000)

#define ARGONLV_WD_DEBUG                 0x0002
#define ARGONLV_WD_ENABLE                0x0004
#define ARGONLV_WD_RESET_EN              0x0008
#define ARGONLV_NOT_SW_RESET             0x0010
#define ARGONLV_WD_NOT_ASSERTED          0x0020
#define ARGONLV_WD_OUTPUT_EN             0x0040
#define ARGONLV_WD_TIMEOUT_SHIFT         8

#define ARGONLV_WDOG1_INIT               ((70 << ARGONLV_WD_TIMEOUT_SHIFT) | \
					 ARGONLV_WD_OUTPUT_EN | ARGONLV_WD_NOT_ASSERTED | \
					 ARGONLV_NOT_SW_RESET | ARGONLV_WD_ENABLE | \
					 ARGONLV_WD_DEBUG)

#define ARGONLV_WDOG2_INIT               ((64 << ARGONLV_WD_TIMEOUT_SHIFT) | \
					 ARGONLV_WD_OUTPUT_EN | ARGONLV_WD_NOT_ASSERTED | \
					 ARGONLV_NOT_SW_RESET | ARGONLV_WD_RESET_EN | \
					 ARGONLV_WD_ENABLE | ARGONLV_WD_DEBUG)

/**
 * IIM / unique id section.
 */

typedef struct {
	volatile u32 uid0;
	volatile u32 uid1;
	volatile u32 uid2;
	volatile u32 uid3;
	volatile u32 uid4;
	volatile u32 uid5;
	volatile u32 uid6;
} ARGONLV_IC_ID_REG_T;

#define ARGONLV_IC_ID_REG                (*(volatile ARGONLV_IC_ID_REG_T *) 0x5001D004)
#define ARGONLV_UID_SIZE                 7

/**
 * USB section.
 */

#define USB_MAX_RX_DATA_SIZE             (8192 + 128)
#define USB_MAX_TX_DATA_SIZE             (2048)
#define USB_MAX_PACKET_SIZE              (64)
#define USB_DATA_ARRAY_SIZE              (64)

#define ARGONLV_USB_CORE_BASE            0x50020000
#define ARGONLV_USB_FUNC_BASE            0x50020040
#define ARGONLV_USB_EP_DESC_BASE         0x50020400
#define ARGONLV_USB_DATA_BASE            0x50021000

#define ARGONLV_USB_EP0_OUT_XBUF         0x1000
#define ARGONLV_USB_EP0_IN_XBUF          0x1040
#define ARGONLV_USB_EP2_IN_XBUF          0x1080
#define ARGONLV_USB_EP1_OUT_XBUF         0x10C0
#define ARGONLV_USB_EP1_OUT_YBUF         0x14C0

#define USB_GLOBAL_EP0_OUT_MASK          0x00000001
#define USB_GLOBAL_EP0_IN_MASK           0x00000002
#define USB_GLOBAL_EP1_OUT_MASK          0x00000004
#define USB_GLOBAL_EP1_IN_MASK           0x00000008
#define USB_GLOBAL_EP2_OUT_MASK          0x00000010
#define USB_GLOBAL_EP2_IN_MASK           0x00000020
#define USB_GLOBAL_EP3_OUT_MASK          0x00000040
#define USB_GLOBAL_EP3_IN_MASK           0x00000080

#define USB_CORE_BEMDE_MASK              0x00000004
#define USB_CORE_CRECFG_MASK             0x00000003
#define USB_CORE_CRECFG_FUNC             2
#define USB_CORE_FUNCCLK_MASK            0x00000004
#define USB_CORE_MAINCLK_MASK            0x00000001

#define USB_FUNC_SOFTRESET_MASK          0x00000080
#define USB_FUNC_DEVADDR_MASK            0x0000007F
#define USB_FUNC_RESETDET_MASK           0x00000001
#define USB_FUNC_RESETINT_MASK           0x00000001
#define USB_FUNC_RESETIEN_MASK           0x00000001

#define USB_EP_DESC_STALL_MASK           0x80000000
#define USB_EP_DESC_SETUP_MASK           0x40000000
#define USB_EP_DESC_OVERRUN_MASK         0x20000000
#define USB_EP_DESC_FORMAT_BULK          0x00008000
#define USB_EP_DESC_MAXPKTSIZ_64         0x00400000
#define USB_EP_DESC_TTLBTECNT_MASK       0x001FFFFF
#define USB_EP_DESC_BUFSIZE_SHIFT        21
#define USB_EP_DESC_BUFSIZE_64           (63 << USB_EP_DESC_BUFSIZE_SHIFT)

#define USB_MFP_RX_EP                    USB_EP1_OUT
#define USB_MFP_RX_MASK                  USB_GLOBAL_EP1_OUT_MASK
#define USB_MFP_TX_EP                    USB_EP2_IN
#define USB_MFP_TX_MASK                  USB_GLOBAL_EP2_IN_MASK
#define USB_MFP_RX_TOTAL_BYTES           0x200C

typedef struct {
	volatile u32 hw_mode_reg;
	volatile u32 core_int_status_reg;
	volatile u32 core_int_en_reg;
	volatile u32 clock_ctrl_reg;
	volatile u32 reset_ctrl_reg;
	volatile u32 frame_interval_reg;
	volatile u32 frame_remaining_reg;
	volatile u32 hnp_ctrl_status_reg;
	volatile u32 hnp_timer1_reg;
	volatile u32 hnp_timer2_reg;
	volatile u32 hnp_timer3_plus_ctrl_reg;
	volatile u32 hnp_int_status_reg;
	volatile u32 hnp_int_ena_status_reg;
	volatile u32 cpu_ep_sel_reg;
	volatile u32 dsp_ep_sel_reg;
	volatile u32 core_int_status_en_clr_reg;
} ARGONLV_USB_CORE_REG_T;

typedef struct {
	volatile u32 func_cmd_status_reg;
	volatile u32 dev_addr_reg;
	volatile u32 sys_int_status_reg;
	volatile u32 sys_int_en_reg;
	volatile u32 xbuff_int_status_reg;
	volatile u32 ybuff_int_status_reg;
	volatile u32 xy_int_en_reg;
	volatile u32 xfilled_status_reg;
	volatile u32 yfilled_status_reg;
	volatile u32 ep_en_reg;
	volatile u32 ep_ready_reg;
	volatile u32 immediate_int_reg;
	volatile u32 ep_done_status_reg;
	volatile u32 ep_done_en_reg;
	volatile u32 ep_tog_bits_reg;
	volatile u32 frame_num_reg;
	volatile u32 reserved1[51];
	volatile u32 sys_int_en_clr_reg;
	volatile u32 reserved2[2];
	volatile u32 xy_int_en_clr_reg;
	volatile u32 reserved3[2];
	volatile u32 ep_en_clr_reg;
	volatile u32 reserved4[1];
	volatile u32 immediate_int_clr_reg;
	volatile u32 reserved5[1];
	volatile u32 ep_done_en_clr_reg;
} ARGONLV_USB_FUNC_REG_T;

typedef struct {
	volatile u32 dword0;
	volatile u32 dword1;
	volatile u32 dword2;
	volatile u32 dword3;
} ARGONLV_USB_EP_DESC_T;

enum {
	USB_EP0_OUT,
	USB_EP0_IN,
	USB_EP1_OUT,
	USB_EP1_IN,
	USB_EP2_OUT,
	USB_EP2_IN,
	USB_EP3_OUT,
	USB_EP3_IN
};

#define ARGONLV_USB_CORE                 (*(volatile ARGONLV_USB_CORE_REG_T *) ARGONLV_USB_CORE_BASE)
#define ARGONLV_USB_FUNC                 (*(volatile ARGONLV_USB_FUNC_REG_T *) ARGONLV_USB_FUNC_BASE)
#define ARGONLV_USB_EP_DESC              ((volatile ARGONLV_USB_EP_DESC_T *) ARGONLV_USB_EP_DESC_BASE)

#define ARGONLV_USB_EP_XBUF(ep)          ((volatile u32 *) (ARGONLV_USB_CORE_BASE + \
					 (ARGONLV_USB_EP_DESC[(ep)].dword1 & 0x0000FFFF)))

#define ARGONLV_USB_EP_YBUF(ep)          ((volatile u32 *) (ARGONLV_USB_CORE_BASE + \
					 ((ARGONLV_USB_EP_DESC[(ep)].dword1 >> 16) & 0x0000FFFF)))

#endif /* !REGS_ARGONLV_H */
