// SPDX-License-Identifier: MIT
// USB identity module: presents a configurable USB device identity
// (VID/PID/bcdDevice/strings) AND device class on the ESP32-S3 native USB port.
#pragma once

#include <stdint.h>
#include "esp_err.h"

#define USB_ID_STR_LEN   32
#define USB_ID_NAME_LEN  24

// Which USB device class this profile presents. The class decides the
// configuration descriptor (interfaces/endpoints) and therefore which host
// driver binds. HID classes enumerate and bind only -- they never emit input.
typedef enum {
    USB_CLASS_VENDOR = 0,   // bare vendor-specific interface, no endpoints (identity only)
    USB_CLASS_HID_KEYBOARD, // boot keyboard  -> host HID driver
    USB_CLASS_HID_MOUSE,    // boot mouse     -> host HID driver
    USB_CLASS_HID_GENERIC,  // generic in/out HID ("hardware peripheral")
    USB_CLASS_CDC_SERIAL,   // virtual COM port -> usbser.sys / cdc-acm
    USB_CLASS_PRINTER,      // printer class 0x07 -> usbprint.sys / usblp
} usb_class_t;

// A device identity: the "who am I" plus "what am I" the host sees at enumeration.
typedef struct {
    char        name[USB_ID_NAME_LEN];        // human label for this profile
    usb_class_t dev_class;                     // which class to present
    uint16_t    vid;                           // idVendor
    uint16_t    pid;                           // idProduct
    uint16_t    bcd_device;                    // bcdDevice (device release, BCD)
    char        manufacturer[USB_ID_STR_LEN];  // iManufacturer string
    char        product[USB_ID_STR_LEN];       // iProduct string
    char        serial[USB_ID_STR_LEN];        // iSerialNumber string
} usb_identity_t;

// Install the USB device stack and begin presenting `id`. Call once at boot.
esp_err_t usb_identity_start(const usb_identity_t *id);

// Copy `id` into the live descriptors (identity + class). Does NOT re-enumerate;
// the change becomes visible on the next enumeration.
void usb_identity_set(const usb_identity_t *id);

// Force the host to tear down and re-read descriptors (soft unplug/replug on D+).
void usb_identity_reenumerate(void);

// Pointer to the current live identity.
const usb_identity_t *usb_identity_get(void);

// Persistence in NVS (survives reboot/power loss).
esp_err_t usb_identity_nvs_save(const usb_identity_t *id);
esp_err_t usb_identity_nvs_load(usb_identity_t *id);  // ESP_ERR_NVS_NOT_FOUND if none
