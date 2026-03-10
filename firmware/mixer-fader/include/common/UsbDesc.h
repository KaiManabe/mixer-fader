#ifndef _USBDESC_H_
#define _USBDESC_H_

/* ========================= USB Device Identifiers ========================== */

#define USB_VID  0xCAFE
#define USB_PID  0x4000

/* =========================== Endpoint Definitions ========================== */

#define USB_EP_OUT         0x01
#define USB_EP_IN          0x82
#define USB_VENDOR_EPSIZE  64

/* ============================ Interface Numbers ============================ */

#define USB_ITF_NUM_VENDOR  0
#define USB_ITF_NUM_TOTAL   1

/* ======================== String Descriptor Indices ======================== */

#define USB_STR_LANG          0
#define USB_STR_MANUFACTURER  1
#define USB_STR_PRODUCT       2
#define USB_STR_SERIAL        3
#define USB_STR_INTERFACE     4

/* ===================== DeviceInterfaceGUID (WinUSB) ======================== */
/*  {d2f46ddb-4f60-4307-8e73-bb2e8cbe6eb4}                                   */

#define USB_DEVICE_INTERFACE_GUID_DATA1  0xd2f46ddb
#define USB_DEVICE_INTERFACE_GUID_DATA2  0x4f60
#define USB_DEVICE_INTERFACE_GUID_DATA3  0x4307
#define USB_DEVICE_INTERFACE_GUID_DATA4  { 0x8e, 0x73, 0xbb, 0x2e, 0x8c, 0xbe, 0x6e, 0xb4 }

#define USB_DEVICE_INTERFACE_GUID_STR    "{d2f46ddb-4f60-4307-8e73-bb2e8cbe6eb4}"

/* UTF-16LE byte sequence for MS OS 2.0 descriptor (with double-null terminator) */
#define USB_DEVICE_INTERFACE_GUID_UTF16LE \
    '{', 0x00, 'd', 0x00, '2', 0x00, 'f', 0x00, '4', 0x00, '6', 0x00, \
    'd', 0x00, 'd', 0x00, 'b', 0x00, '-', 0x00, '4', 0x00, 'f', 0x00, \
    '6', 0x00, '0', 0x00, '-', 0x00, '4', 0x00, '3', 0x00, '0', 0x00, \
    '7', 0x00, '-', 0x00, '8', 0x00, 'e', 0x00, '7', 0x00, '3', 0x00, \
    '-', 0x00, 'b', 0x00, 'b', 0x00, '2', 0x00, 'e', 0x00, '8', 0x00, \
    'c', 0x00, 'b', 0x00, 'e', 0x00, '6', 0x00, 'e', 0x00, 'b', 0x00, \
    '4', 0x00, '}', 0x00, 0x00, 0x00, 0x00, 0x00

#endif
