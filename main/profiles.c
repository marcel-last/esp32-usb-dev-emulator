// SPDX-License-Identifier: MIT
#include "profiles.h"

// Preset identities. Each carries a device class -> the OS binds the matching
// in-box driver (no download) and the VID/PID steers which specific model.
//
// "WU-software" entries use real VID:PID pairs that Windows Update matches to
// OEM companion apps (Synapse, G Hub, iCUE, etc.). The class descriptor is
// enough for the in-box HID driver to load immediately; WU then installs the
// OEM software in the background on default Windows 10/11.  The software install
// occurs before any vendor-protocol handshake, so it completes even though this
// firmware never sends a vendor HID report.
//
// VID:PID pairs are public USB-IF-registered identifiers.
// Serials are dummy placeholders; real devices vary.
const usb_identity_t USB_PROFILES[] = {

    // -----------------------------------------------------------------------
    // Lab / baseline
    // -----------------------------------------------------------------------
    {
        .name = "esp-default", .dev_class = USB_CLASS_VENDOR,
        .vid = 0x303A, .pid = 0x4001, .bcd_device = 0x0100,
        .manufacturer = "Espressif", .product = "ESP32-S3 Device", .serial = "S3-LAB-0001",
    },
    {
        .name = "hid-generic", .dev_class = USB_CLASS_HID_GENERIC,
        .vid = 0x1209, .pid = 0x0002, .bcd_device = 0x0100,
        .manufacturer = "Lab", .product = "Generic HID Peripheral", .serial = "HID-01",
    },
    {
        .name = "custom", .dev_class = USB_CLASS_VENDOR,
        .vid = 0x1209, .pid = 0x0001, .bcd_device = 0x0100,
        .manufacturer = "Lab", .product = "Custom Device", .serial = "CUSTOM-01",
    },

    // -----------------------------------------------------------------------
    // Serial adapters
    // -----------------------------------------------------------------------
    {
        .name = "ftdi-serial", .dev_class = USB_CLASS_CDC_SERIAL,
        .vid = 0x0403, .pid = 0x6001, .bcd_device = 0x0600,
        .manufacturer = "FTDI", .product = "FT232 USB-Serial", .serial = "A50285BI",
    },
    {
        .name = "cp2102-serial", .dev_class = USB_CLASS_CDC_SERIAL,
        .vid = 0x10C4, .pid = 0xEA60, .bcd_device = 0x0100,
        .manufacturer = "Silicon Labs", .product = "CP2102 USB to UART", .serial = "0001",
    },
    {
        .name = "arduino-uno", .dev_class = USB_CLASS_CDC_SERIAL,
        .vid = 0x2341, .pid = 0x0043, .bcd_device = 0x0001,
        .manufacturer = "Arduino LLC", .product = "Arduino Uno", .serial = "",
    },

    // -----------------------------------------------------------------------
    // Generic HID peripherals (class driver only, no WU software)
    // -----------------------------------------------------------------------
    {
        .name = "cherry-kbd", .dev_class = USB_CLASS_HID_KEYBOARD,
        .vid = 0x046A, .pid = 0x0011, .bcd_device = 0x0100,
        .manufacturer = "Cherry", .product = "Keyboard", .serial = "",
    },
    {
        .name = "logi-mouse", .dev_class = USB_CLASS_HID_MOUSE,
        .vid = 0x046D, .pid = 0xC077, .bcd_device = 0x7200,
        .manufacturer = "Logitech", .product = "USB Optical Mouse", .serial = "",
    },

    // -----------------------------------------------------------------------
    // Printers
    // -----------------------------------------------------------------------
    {
        .name = "hp-laserjet", .dev_class = USB_CLASS_PRINTER,
        .vid = 0x03F0, .pid = 0x0417, .bcd_device = 0x0100,
        .manufacturer = "HP", .product = "LaserJet 1018", .serial = "LAB-PRN-01",
    },
    // HP Easy Start / HP Smart pulled by WU on VID:PID match
    {
        .name = "hp-deskjet", .dev_class = USB_CLASS_PRINTER,
        .vid = 0x03F0, .pid = 0x4817, .bcd_device = 0x0100,
        .manufacturer = "HP", .product = "DeskJet 2700", .serial = "LAB-PRN-02",
    },

    // -----------------------------------------------------------------------
    // Razer  (VID 0x1532) -- WU pushes Razer Synapse 3
    // -----------------------------------------------------------------------
    // BlackWidow V3 -- one of the most common Synapse triggers in the WU catalog
    {
        .name = "razer-blackwidow", .dev_class = USB_CLASS_HID_KEYBOARD,
        .vid = 0x1532, .pid = 0x0225, .bcd_device = 0x0200,
        .manufacturer = "Razer", .product = "BlackWidow V3", .serial = "RZ-LAB-0001",
    },
    // DeathAdder V2 mouse
    {
        .name = "razer-deathadder", .dev_class = USB_CLASS_HID_MOUSE,
        .vid = 0x1532, .pid = 0x0084, .bcd_device = 0x0200,
        .manufacturer = "Razer", .product = "DeathAdder V2", .serial = "RZ-LAB-0002",
    },
    // Kraken headset (HID audio control interface exposed as generic HID on this PID)
    {
        .name = "razer-kraken", .dev_class = USB_CLASS_HID_GENERIC,
        .vid = 0x1532, .pid = 0x0527, .bcd_device = 0x0200,
        .manufacturer = "Razer", .product = "Kraken V3 HyperSense", .serial = "RZ-LAB-0003",
    },
    // Viper V2 Pro mouse
    {
        .name = "razer-viper", .dev_class = USB_CLASS_HID_MOUSE,
        .vid = 0x1532, .pid = 0x00A5, .bcd_device = 0x0200,
        .manufacturer = "Razer", .product = "Viper V2 Pro", .serial = "RZ-LAB-0004",
    },

    // -----------------------------------------------------------------------
    // Logitech  (VID 0x046D) -- WU pushes Logitech G Hub or Options+
    // -----------------------------------------------------------------------
    // G Pro X Superlight 2 mouse
    {
        .name = "logi-gpro", .dev_class = USB_CLASS_HID_MOUSE,
        .vid = 0x046D, .pid = 0xC099, .bcd_device = 0x0002,
        .manufacturer = "Logitech", .product = "G Pro X Superlight 2", .serial = "LG-LAB-0001",
    },
    // G915 TKL keyboard
    {
        .name = "logi-g915", .dev_class = USB_CLASS_HID_KEYBOARD,
        .vid = 0x046D, .pid = 0xC343, .bcd_device = 0x0001,
        .manufacturer = "Logitech", .product = "G915 TKL Keyboard", .serial = "LG-LAB-0002",
    },
    // G502 X Plus mouse
    {
        .name = "logi-g502", .dev_class = USB_CLASS_HID_MOUSE,
        .vid = 0x046D, .pid = 0xC101, .bcd_device = 0x0001,
        .manufacturer = "Logitech", .product = "G502 X Plus", .serial = "LG-LAB-0003",
    },
    // MX Master 3 (Options+ software)
    {
        .name = "logi-mxmaster", .dev_class = USB_CLASS_HID_MOUSE,
        .vid = 0x046D, .pid = 0xC08B, .bcd_device = 0x0012,
        .manufacturer = "Logitech", .product = "MX Master 3", .serial = "LG-LAB-0004",
    },

    // -----------------------------------------------------------------------
    // Corsair  (VID 0x1B1C) -- WU pushes Corsair iCUE
    // -----------------------------------------------------------------------
    // K100 RGB keyboard
    {
        .name = "corsair-k100", .dev_class = USB_CLASS_HID_KEYBOARD,
        .vid = 0x1B1C, .pid = 0x1B7E, .bcd_device = 0x0300,
        .manufacturer = "Corsair", .product = "K100 RGB", .serial = "CS-LAB-0001",
    },
    // Scimitar Elite mouse
    {
        .name = "corsair-scimitar", .dev_class = USB_CLASS_HID_MOUSE,
        .vid = 0x1B1C, .pid = 0x1BAE, .bcd_device = 0x0300,
        .manufacturer = "Corsair", .product = "Scimitar Elite RGB", .serial = "CS-LAB-0002",
    },
    // HS80 RGB headset (HID control interface)
    {
        .name = "corsair-hs80", .dev_class = USB_CLASS_HID_GENERIC,
        .vid = 0x1B1C, .pid = 0x0A67, .bcd_device = 0x0100,
        .manufacturer = "Corsair", .product = "HS80 RGB Wireless", .serial = "CS-LAB-0003",
    },

    // -----------------------------------------------------------------------
    // SteelSeries  (VID 0x1038) -- WU pushes SteelSeries GG
    // -----------------------------------------------------------------------
    // Apex Pro keyboard
    {
        .name = "ss-apex", .dev_class = USB_CLASS_HID_KEYBOARD,
        .vid = 0x1038, .pid = 0x1610, .bcd_device = 0x0300,
        .manufacturer = "SteelSeries", .product = "Apex Pro", .serial = "SS-LAB-0001",
    },
    // Rival 650 mouse
    {
        .name = "ss-rival", .dev_class = USB_CLASS_HID_MOUSE,
        .vid = 0x1038, .pid = 0x1726, .bcd_device = 0x0300,
        .manufacturer = "SteelSeries", .product = "Rival 650 Wireless", .serial = "SS-LAB-0002",
    },
    // Arctis Nova Pro headset
    {
        .name = "ss-arctis", .dev_class = USB_CLASS_HID_GENERIC,
        .vid = 0x1038, .pid = 0x12C0, .bcd_device = 0x0100,
        .manufacturer = "SteelSeries", .product = "Arctis Nova Pro", .serial = "SS-LAB-0003",
    },

    // -----------------------------------------------------------------------
    // ASUS ROG  (VID 0x0B05) -- WU pushes Armoury Crate
    // -----------------------------------------------------------------------
    // ROG Strix Scope keyboard
    {
        .name = "asus-scope", .dev_class = USB_CLASS_HID_KEYBOARD,
        .vid = 0x0B05, .pid = 0x1866, .bcd_device = 0x0100,
        .manufacturer = "ASUSTeK", .product = "ROG Strix Scope", .serial = "AS-LAB-0001",
    },
    // ROG Chakram X mouse
    {
        .name = "asus-chakram", .dev_class = USB_CLASS_HID_MOUSE,
        .vid = 0x0B05, .pid = 0x1A18, .bcd_device = 0x0100,
        .manufacturer = "ASUSTeK", .product = "ROG Chakram X", .serial = "AS-LAB-0002",
    },
    // ROG Delta headset (HID control interface)
    {
        .name = "asus-delta", .dev_class = USB_CLASS_HID_GENERIC,
        .vid = 0x0B05, .pid = 0x17F8, .bcd_device = 0x0100,
        .manufacturer = "ASUSTeK", .product = "ROG Delta Headset", .serial = "AS-LAB-0003",
    },
};

const size_t USB_PROFILE_COUNT = sizeof(USB_PROFILES) / sizeof(USB_PROFILES[0]);
