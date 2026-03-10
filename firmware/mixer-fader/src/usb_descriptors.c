#include "tusb.h"
#include "common/UsbDesc.h"

/* ============================= Device Descriptor ============================== */
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
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

uint8_t const desc_configuration[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, USB_ITF_NUM_TOTAL, 0, 34, 0x80, 250),
    
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
