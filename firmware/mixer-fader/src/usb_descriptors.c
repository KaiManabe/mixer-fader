#include "tusb.h"
#include "common/UsbDesc.h"

/* ============================= String Descriptors ============================== */

static const uint16_t desc_str_lan[] = {
    (TUSB_DESC_STRING << 8) | 4,
    0x0409  // English (US)
};

static const uint16_t desc_str_manufact[] = {
    (TUSB_DESC_STRING << 8) | (2 + 12*2),
    'R', 'a', 's', 'p', 'b', 'e', 'r', 'r', 'y', ' ', 'P', 'i'
};

static const uint16_t desc_str_product[] = {
    (TUSB_DESC_STRING << 8) | (2 + 15*2),
    'M', 'i', 'x', 'e', 'r', ' ', 'F', 'a', 'd', 'e', 'r', ' ', 'D', 'e', 'v'
};

static const uint16_t desc_str_serial[] = {
    (TUSB_DESC_STRING << 8) | (2 + 12*2),
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', 'A', 'B'
};

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

/* ==================== Microsoft OS 2.0 Descriptors ========================= */

// Vendor request code advertised in BOS descriptor
#define MSOS20_VENDOR_CODE        0x01
#define MS_OS_20_DESCRIPTOR_INDEX 0x07

// Total size of MS OS 2.0 Descriptor Set
//   Set Header (10) + Compatible ID (20) + Registry Property (132) = 162
#define MS_OS_20_DESC_LEN 162

static uint8_t const desc_ms_os_20[MS_OS_20_DESC_LEN] = {
    /* ---- MS OS 2.0 Descriptor Set Header (10 bytes) ---- */
    0x0A, 0x00,                         // wLength
    0x00, 0x00,                         // wDescriptorType: SET_HEADER
    0x00, 0x00, 0x03, 0x06,             // dwWindowsVersion: Win 8.1+ (0x06030000)
    0xA2, 0x00,                         // wTotalLength: 162

    /* ---- MS OS 2.0 Compatible ID Descriptor (20 bytes) ---- */
    0x14, 0x00,                         // wLength
    0x03, 0x00,                         // wDescriptorType: COMPATIBLE_ID
    'W',  'I',  'N',  'U',  'S',  'B',  0x00, 0x00,   // CompatibleID
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // SubCompatibleID

    /* ---- MS OS 2.0 Registry Property Descriptor (132 bytes) ---- */
    0x84, 0x00,                         // wLength: 132
    0x04, 0x00,                         // wDescriptorType: REG_PROPERTY
    0x07, 0x00,                         // wPropertyDataType: REG_MULTI_SZ
    0x2A, 0x00,                         // wPropertyNameLength: 42
    // PropertyName: "DeviceInterfaceGUIDs\0" (UTF-16LE, 42 bytes)
    'D', 0x00, 'e', 0x00, 'v', 0x00, 'i', 0x00, 'c', 0x00, 'e', 0x00,
    'I', 0x00, 'n', 0x00, 't', 0x00, 'e', 0x00, 'r', 0x00, 'f', 0x00,
    'a', 0x00, 'c', 0x00, 'e', 0x00, 'G', 0x00, 'U', 0x00, 'I', 0x00,
    'D', 0x00, 's', 0x00, 0x00, 0x00,
    0x50, 0x00,                         // wPropertyDataLength: 80
    // PropertyData: USB_DEVICE_INTERFACE_GUID_STR (UTF-16LE, 80 bytes)
    USB_DEVICE_INTERFACE_GUID_UTF16LE,
};

// BOS Descriptor: 5 (header) + 28 (MS OS 2.0 capability) = 33 bytes
#define BOS_TOTAL_LEN 33

static uint8_t const desc_bos[BOS_TOTAL_LEN] = {
    /* ---- BOS Descriptor Header (5 bytes) ---- */
    0x05, 0x0F,                         // bLength, bDescriptorType: BOS
    0x21, 0x00,                         // wTotalLength: 33
    0x01,                               // bNumDeviceCaps: 1

    /* ---- MS OS 2.0 Platform Capability (28 bytes) ---- */
    0x1C,                               // bLength: 28
    0x10,                               // bDescriptorType: DEVICE_CAPABILITY
    0x05,                               // bDevCapabilityType: PLATFORM
    0x00,                               // bReserved
    // PlatformCapabilityUUID: {D8DD60DF-4589-4CC7-9CD2-659D9E648A9F}
    0xDF, 0x60, 0xDD, 0xD8, 0x89, 0x45, 0xC7, 0x4C,
    0x9C, 0xD2, 0x65, 0x9D, 0x9E, 0x64, 0x8A, 0x9F,
    // Vendor-specific: MS OS 2.0 descriptor info
    0x00, 0x00, 0x03, 0x06,             // dwWindowsVersion: Win 8.1+
    MS_OS_20_DESC_LEN & 0xFF,           // wMSOSDescriptorSetTotalLength (low)
    (MS_OS_20_DESC_LEN >> 8) & 0xFF,    // wMSOSDescriptorSetTotalLength (high)
    MSOS20_VENDOR_CODE,                 // bMS_VendorCode
    0x00,                               // bAltEnumCode
};

/* ============================= Device Descriptor ============================== */
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0210,  // USB 2.1 required for BOS descriptor
    .bDeviceClass       = 0,
    .bDeviceSubClass    = 0,
    .bDeviceProtocol    = 0,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,
    .iManufacturer      = USB_STR_MANUFACTURER,
    .iProduct           = USB_STR_PRODUCT,
    .iSerialNumber      = USB_STR_SERIAL,
    .bNumConfigurations = 0x01
};

/* ============================= Configuration Descriptor ============================== */

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_VENDOR_DESC_LEN)

uint8_t const desc_configuration[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, USB_ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x80, 250),
    
    // Interface number, string index, EP OUT & IN address, EP size
    TUD_VENDOR_DESCRIPTOR(USB_ITF_NUM_VENDOR, USB_STR_INTERFACE, USB_EP_OUT, USB_EP_IN, USB_VENDOR_EPSIZE),
};

/* ============================= String Descriptors ============================== */

static uint16_t _desc_str[32];

uint8_t const *tud_descriptor_device_cb(void) {
    return (uint8_t const *)&desc_device;
}

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return desc_configuration;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    
    uint8_t chr_count = 0;
    
    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0] + 1, 2);
        chr_count = 1;
    } else {
        if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))) return NULL;
        
        const uint16_t *desc = string_desc_arr[index];
        chr_count = (uint8_t)(((uint8_t)(desc[0] & 0x00FF) - 2u) / 2u);
        memcpy(&_desc_str[1], desc + 1, chr_count * 2);
    }
    
    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 + chr_count * 2);
    
    return _desc_str;
}

/* ============================ BOS Descriptor =============================== */

uint8_t const *tud_descriptor_bos_cb(void) {
    return desc_bos;
}

/* ================ MS OS 2.0 Vendor Request Handler ========================= */

bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage,
                                tusb_control_request_t const *request) {
    // Handle MS OS 2.0 descriptor request
    if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_VENDOR &&
        request->bRequest == MSOS20_VENDOR_CODE &&
        request->wIndex == MS_OS_20_DESCRIPTOR_INDEX) {
        if (stage == CONTROL_STAGE_SETUP) {
            return tud_control_xfer(rhport, request,
                                    (void *)(uintptr_t)desc_ms_os_20,
                                    sizeof(desc_ms_os_20));
        }
        return true;  // ACK for DATA / ACK stages
    }

    return false;  // stall unknown requests
}
