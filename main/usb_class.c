// SPDX-License-Identifier: MIT
#include <string.h>
#include "usb_class.h"
#include "tinyusb.h"   // pulls in tusb.h: TUD_* macros, HID report macros, types

// ===========================================================================
// HID report descriptors (one per HID class). Returned verbatim to the host;
// their contents are what make the OS load the keyboard/mouse/HID driver.
// The firmware never SENDS a report -- these are for binding only.
// ===========================================================================
static const uint8_t s_report_kbd[]     = { TUD_HID_REPORT_DESC_KEYBOARD() };
static const uint8_t s_report_mouse[]   = { TUD_HID_REPORT_DESC_MOUSE() };
static const uint8_t s_report_generic[] = { TUD_HID_REPORT_DESC_GENERIC_INOUT(CFG_TUD_HID_EP_BUFSIZE) };

// Endpoint addresses (only one class is ever active per enumeration, so reuse
// is fine). IN endpoints have bit7 set.
#define EP_IN1   0x81
#define EP_OUT1  0x01
#define EP_OUT2  0x02
#define EP_IN2   0x82

// ===========================================================================
// Configuration descriptors, one per class. Built at compile time with the
// TinyUSB descriptor macros so lengths and layout are correct by construction.
// bmAttributes arg is 0x00 here -- the macro sets the mandatory bit7 itself.
// ===========================================================================

// --- HID keyboard (boot protocol so it binds even at BIOS/UEFI) ---
#define CFG_LEN_KBD  (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)
static const uint8_t cfg_kbd[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, CFG_LEN_KBD, 0x00, 100),
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_KEYBOARD, sizeof(s_report_kbd),
                       EP_IN1, CFG_TUD_HID_EP_BUFSIZE, 10),
};

// --- HID mouse (boot protocol) ---
#define CFG_LEN_MOUSE  (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)
static const uint8_t cfg_mouse[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, CFG_LEN_MOUSE, 0x00, 100),
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_MOUSE, sizeof(s_report_mouse),
                       EP_IN1, CFG_TUD_HID_EP_BUFSIZE, 10),
};

// --- Generic in/out HID ("hardware peripheral": vendor-defined HID, IN + OUT) ---
#define CFG_LEN_GENERIC  (TUD_CONFIG_DESC_LEN + TUD_HID_INOUT_DESC_LEN)
static const uint8_t cfg_generic[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, CFG_LEN_GENERIC, 0x00, 100),
    TUD_HID_INOUT_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, sizeof(s_report_generic),
                             EP_OUT1, EP_IN1, CFG_TUD_HID_EP_BUFSIZE, 10),
};

// --- CDC-ACM serial (virtual COM port) ---
#define CFG_LEN_CDC  (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN)
static const uint8_t cfg_cdc[] = {
    TUD_CONFIG_DESCRIPTOR(1, 2, 0, CFG_LEN_CDC, 0x00, 100),
    TUD_CDC_DESCRIPTOR(0, 0, EP_IN1, 8, EP_OUT2, EP_IN2, 64),
};

// --- Printer, class 0x07 (hand-built: TinyUSB has no printer class macro) ---
// Interface class 7 / subclass 1 / protocol 2 (bidirectional) + bulk OUT & IN.
// The endpoints are serviced, and GET_DEVICE_ID answered, by usb_app_driver.c.
#define CFG_LEN_PRN  (TUD_CONFIG_DESC_LEN + 9 + 7 + 7)
static const uint8_t cfg_prn[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, CFG_LEN_PRN, 0x00, 100),
    // Interface descriptor
    9, TUSB_DESC_INTERFACE, 0, 0, 2, 0x07, 0x01, 0x02, 0,
    // Bulk OUT endpoint (host -> device print data)
    7, TUSB_DESC_ENDPOINT, EP_OUT1, TUSB_XFER_BULK, 0x40, 0x00, 0,
    // Bulk IN endpoint (device -> host status / 1284 data)
    7, TUSB_DESC_ENDPOINT, EP_IN1, TUSB_XFER_BULK, 0x40, 0x00, 0,
};

// --- Vendor-specific identity (class 0xFF, zero endpoints) ---
// Claimed by usb_app_driver.c so SET_CONFIGURATION succeeds cleanly.
#define CFG_LEN_VENDOR  (TUD_CONFIG_DESC_LEN + 9)
static const uint8_t cfg_vendor[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, CFG_LEN_VENDOR, 0x00, 100),
    9, TUSB_DESC_INTERFACE, 0, 0, 0, 0xFF, 0x00, 0x00, 0,
};

// ===========================================================================
// Active-class tracking + selection
// ===========================================================================
static usb_class_t s_active_class = USB_CLASS_VENDOR;

// Exposed for usb_app_driver.c so the printer 1284 ID reflects the live profile.
usb_class_t usb_class_active(void) { return s_active_class; }

size_t usb_class_select(usb_class_t cls, uint8_t *out, size_t cap, uint8_t dev_codes[3])
{
    const uint8_t *src;
    size_t len;

    dev_codes[0] = 0x00; dev_codes[1] = 0x00; dev_codes[2] = 0x00;

    switch (cls) {
        case USB_CLASS_HID_KEYBOARD: src = cfg_kbd;     len = sizeof(cfg_kbd);     break;
        case USB_CLASS_HID_MOUSE:    src = cfg_mouse;   len = sizeof(cfg_mouse);   break;
        case USB_CLASS_HID_GENERIC:  src = cfg_generic; len = sizeof(cfg_generic); break;
        case USB_CLASS_CDC_SERIAL:   src = cfg_cdc;     len = sizeof(cfg_cdc);
            // CDC is a composite (IAD) device -> class codes at device level.
            dev_codes[0] = 0xEF; dev_codes[1] = 0x02; dev_codes[2] = 0x01; break;
        case USB_CLASS_PRINTER:      src = cfg_prn;     len = sizeof(cfg_prn);     break;
        case USB_CLASS_VENDOR:
        default:                     src = cfg_vendor;  len = sizeof(cfg_vendor);  break;
    }

    if (len > cap) {
        return 0;
    }
    memcpy(out, src, len);
    s_active_class = cls;
    return len;
}

// ===========================================================================
// TinyUSB HID callbacks. esp_tinyusb does NOT own these -- the app provides
// them. Report descriptor is chosen by the active class; input is never sent.
// ===========================================================================
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;
    switch (s_active_class) {
        case USB_CLASS_HID_MOUSE:   return s_report_mouse;
        case USB_CLASS_HID_GENERIC: return s_report_generic;
        default:                    return s_report_kbd;
    }
}

// GET_REPORT: we present no input, so return 0 (stack STALLs -- harmless).
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t report_type,
                               uint8_t *buffer, uint16_t reqlen)
{
    (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)reqlen;
    return 0;
}

// SET_REPORT (e.g. keyboard LED state from host): accept and ignore.
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t bufsize)
{
    (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)bufsize;
}

// ===========================================================================
// Name <-> enum
// ===========================================================================
static const struct { usb_class_t c; const char *n; } k_names[] = {
    { USB_CLASS_VENDOR,       "vendor"      },
    { USB_CLASS_HID_KEYBOARD, "keyboard"    },
    { USB_CLASS_HID_MOUSE,    "mouse"       },
    { USB_CLASS_HID_GENERIC,  "hid-generic" },
    { USB_CLASS_CDC_SERIAL,   "serial"      },
    { USB_CLASS_PRINTER,      "printer"     },
};

const char *usb_class_name(usb_class_t cls)
{
    for (size_t i = 0; i < sizeof(k_names) / sizeof(k_names[0]); i++) {
        if (k_names[i].c == cls) return k_names[i].n;
    }
    return "vendor";
}

bool usb_class_from_name(const char *name, usb_class_t *out)
{
    for (size_t i = 0; i < sizeof(k_names) / sizeof(k_names[0]); i++) {
        if (strcmp(k_names[i].n, name) == 0) { *out = k_names[i].c; return true; }
    }
    return false;
}
