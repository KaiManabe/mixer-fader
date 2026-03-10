#ifndef _USBDESC_H_
#define _USBDESC_H_

#include <stdint.h>
#include <string.h>
#include "tusb.h"

// USB VID/PID
static const uint16_t USB_VID = 0xcafe;
static const uint16_t USB_PID = 0x4000;

// Endpoint definitions
static const uint8_t USB_EP_OUT = 0x01;
static const uint8_t USB_EP_IN  = 0x82;
static const uint8_t USB_VENDOR_EPSIZE = 64;

// Interface numbers
static const uint8_t USB_ITF_NUM_VENDOR = 0;
static const uint8_t USB_ITF_NUM_TOTAL = 1;

// String descriptor indices
static const uint8_t USB_STR_LANG = 0;
static const uint8_t USB_STR_MANUFACTURER = 1;
static const uint8_t USB_STR_PRODUCT = 2;
static const uint8_t USB_STR_SERIAL = 3;
static const uint8_t USB_STR_INTERFACE = 4;

// ============================= String Descriptors ============================== //

// String descriptor 0: Language
static const uint16_t desc_str_lan[] = {
    (TUSB_DESC_STRING << 8) | 4,
    0x0409  // English (US)
};

// String descriptor 1: Manufacturer
static const uint16_t desc_str_manufact[] = {
    (TUSB_DESC_STRING << 8) | (2 + 12*2),
    'R', 'a', 's', 'p', 'b', 'e', 'r', 'r', 'y', ' ', 'P', 'i'
};

// String descriptor 2: Product
static const uint16_t desc_str_product[] = {
    (TUSB_DESC_STRING << 8) | (2 + 13*2),
    'M', 'i', 'x', 'e', 'r', ' ', 'F', 'a', 'd', 'e', 'r', ' ', 'D', 'e', 'v'
};

// String descriptor 3: Serial Number
static const uint16_t desc_str_serial[] = {
    (TUSB_DESC_STRING << 8) | (2 + 12*2),
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', 'A', 'B'
};

// String descriptor 4: Interface
static const uint16_t desc_str_interface[] = {
    (TUSB_DESC_STRING << 8) | (2 + 6*2),
    'C', 'u', 's', 't', 'o', 'm'
};

static const uint16_t *string_desc_arr[] = {
    desc_str_lan,
    desc_str_manufact,
    desc_str_product,
    desc_str_serial,
    desc_str_interface
};

#endif
