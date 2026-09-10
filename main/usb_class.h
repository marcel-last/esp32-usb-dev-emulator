// SPDX-License-Identifier: MIT
// Per-class descriptor builders: turn a usb_class_t into the configuration
// descriptor bytes (and device-descriptor class codes) the host will read.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "usb_identity.h"

// Copy the configuration descriptor for `cls` into `out` (capacity `cap`).
// Writes the device-descriptor class triple [bDeviceClass, subclass, protocol]
// into dev_codes[3]. Records the class as active (used by the HID report
// callback). Returns the descriptor length, or 0 if it would not fit.
size_t usb_class_select(usb_class_t cls, uint8_t *out, size_t cap, uint8_t dev_codes[3]);

// Name <-> enum, for the console and profile listings.
const char *usb_class_name(usb_class_t cls);
bool        usb_class_from_name(const char *name, usb_class_t *out);
