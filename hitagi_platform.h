/*
 * About:
 *   Hitagi platform abstraction layer.
 *
 * License:
 *   MIT
 */

#ifndef HITAGI_PLATFORM_H
#define HITAGI_PLATFORM_H

#include "platform.h"

#if defined(FTR_NEPTUNE_LTE1) || defined(FTR_NEPTUNE_LTE2)
#include "regs_neptune.h"
#elif defined(FTR_ARGONLV)
#include "regs_argonlv.h"
#else
#error "Unsupported Hitagi platform"
#endif

/**
 * Common helpers exported for platform code.
 */

void util_u8_to_hexasc(u8 val, u8 *str);
void util_u16_to_hexasc(u16 val, u8 *str);

/**
 * Platform hooks.
 */

int usb_init(void);
int usb_tx(const u8 *src, u8 len);
u8 usb_rx(u8 *dst);

#if defined(FTR_ARGONLV)
int argonlv_usb_tx_fast(const u8 *src, u8 len);
#endif

int watchdog_reboot(void);
int watchdog_shutdown(void);
int watchdog_service(void);

int hitagi_platform_init(void);
u16 hitagi_platform_bootloader_version(void);
void hitagi_platform_serial_number(u8 *response_ptr);

void hitagi_start(void);

#endif /* !HITAGI_PLATFORM_H */
