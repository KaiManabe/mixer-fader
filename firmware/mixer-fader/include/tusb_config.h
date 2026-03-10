#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
 extern "C" {
#endif

/* ============================= Device Config ============================== */
#define CFG_TUSB_RHPORT0_MODE OPT_MODE_DEVICE

/* USB DMA buffer size (in bytes) */
#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN __attribute__ ((aligned(4)))

/* ============================= Class Config ============================== */

/* Vendor Class Support */
#define CFG_TUD_VENDOR 1

/* Configuration max power in mA */
#define CFG_TUSB_MAX_SPEED OPT_MODE_FULL_SPEED

/* Max packet size for bulk endpoints */
#define CFG_TUD_VENDOR_EPSIZE 64

/* String descriptor support */
#define CFG_TUD_STR_APPLY_MACROS 1

/* ============================= Root Hub Config ============================== */
#define CFG_TUSB_RHT0_MODE OPT_MODE_DEVICE

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CONFIG_H_ */
