# ESP32-S3 USB Device Emulator

A lab tool for USB driver/enumeration testing. When plugged into a host under
test, the ESP32-S3 presents a fully-configured USB device - chosen VID, PID,
strings **and** device class - so the OS binds a real in-box driver without any
download.  Runtime control is via a serial console on the second USB port.

---

## Hardware

**DevKitC-1 has two USB-C ports; both cables must be connected.**

| Port label | GPIO | Role |
|------------|------|------|
| **USB**    | GPIO19/20 (D−/D+) | Native USB → plugged into the **host under test** |
| **UART**   | CP210x bridge → UART0 | Serial console on **your** control machine |

The split lets you reconfigure the emulated device while it is still attached to
the target host.

---

## Implemented classes

| Class name    | USB class code | Host driver loaded                          | Notes |
|---------------|---------------|---------------------------------------------|-------|
| `vendor`      | 0xFF           | none (identity only)                        | Zero endpoints; SET_CONFIG succeeds cleanly |
| `keyboard`    | 0x03 HID       | HID class driver (usbhid / IOUSBHostHIDDevice) | Boot-protocol; never emits keystrokes |
| `mouse`       | 0x03 HID       | HID class driver                            | Boot-protocol; never emits movement |
| `hid-generic` | 0x03 HID       | HID class driver                            | Generic in/out; VID:PID drives model binding |
| `serial`      | 0xEF CDC-ACM   | usbser.sys / cdc-acm.ko                     | IAD composite; creates a COM port / ttyACM |
| `printer`     | 0x07           | usbprint.sys / usblp.ko                     | Answers GET_DEVICE_ID with IEEE-1284 string |

**Enumerate-only, send nothing** - HID classes present valid report descriptors
so the driver loads, but the firmware never sends a report or keystroke.

---

## Build & flash

```bash
. $IDF_PATH/export.sh          # ESP-IDF v5.1+ or v6.x

cd esp32s3-usb-id-emulator
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash   # UART port, NOT the native USB port
```

---

## Example: forcing Razer Synapse installation

This is the primary Windows driver-push test case.  On a default Windows 10/11
machine, plugging in any device with Razer's VID (0x1532) and a HID class
interface triggers a Windows Update query that silently downloads and installs
**Razer Synapse 3** - before any vendor-protocol handshake occurs.

### Step 1 - Use the BlackWidow V3 preset

```
usbid> use razer-blackwidow
Staged preset 'razer-blackwidow' (class=keyboard). Run 'apply' to activate.

usbid> apply
Applied. Presenting 1532:0225 as class 'keyboard' (BlackWidow V3).
```

The host immediately binds the in-box HID keyboard driver and recognises the
device as a keyboard - no download required for that step.

### Step 2 - Watch Windows Update pull Synapse

On the host under test, open Device Manager.  Within 30–120 seconds (depending
on internet speed and WU deferral policy) you will see:

```
Keyboards
  └─ Razer BlackWidow V3       ← in-box HID driver, instant
Other devices / Software
  └─ Razer Synapse 3           ← pulled from Windows Update catalog
```

Synapse installs silently if the MSI is WU-signed; it may show a tray
notification or UAC prompt depending on the machine's elevation policy.

### Step 3 - Override strings to match a specific serial

```
usbid> set serial "PM2152E28500816"
usbid> set mfr "Razer"
usbid> set prod "BlackWidow V3"
usbid> apply
```

Synapse reads the serial number after install to identify the device.  Setting a
realistic serial (format: PM + digits) makes Synapse believe a real device is
present and suppresses its "device not found" nag.

### Step 4 - Switch model mid-session

```
usbid> use razer-deathadder
usbid> apply
```

Re-enumerates as a DeathAdder V2 mouse (~300 ms soft replug).  Synapse is already
installed; it detects the new PID and loads the mouse profile pane.

### Step 5 - Persist across power cycles

```
usbid> save
Saved staged identity to NVS.
```

The device boots as the saved identity next time - no console interaction needed.

---

## All console commands

```
usbid> list                        # show all presets with class and VID:PID
usbid> use <name-or-index>         # stage a preset into the edit buffer
usbid> show                        # diff staged vs currently-enumerated identity
usbid> apply                       # commit edit buffer + force re-enumeration
usbid> save                        # persist edit buffer to NVS
usbid> load                        # load NVS identity into edit buffer
usbid> reboot                      # restart the device

usbid> set vid   <hex>             # e.g.  set vid 1532
usbid> set pid   <hex>             # e.g.  set pid 0067
usbid> set bcddev <hex>            # bcdDevice (device release number)
usbid> set class <name>            # vendor | keyboard | mouse | hid-generic | serial | printer
usbid> set mfr   <string>          # iManufacturer string (max 31 chars)
usbid> set prod  <string>          # iProduct string
usbid> set serial <string>         # iSerialNumber string
usbid> set name  <string>          # local profile label (not sent to host)
```

---

## Preset profiles

```
Index  Name               VID:PID    Class        Product
-----  -----------------  ---------  -----------  ----------------------------
  0    esp-default        303A:4001  vendor       ESP32-S3 Device
  1    hid-generic        1209:0002  hid-generic  Generic HID Peripheral
  2    custom             1209:0001  vendor       Custom Device

  3    ftdi-serial        0403:6001  serial       FT232 USB-Serial
  4    cp2102-serial      10C4:EA60  serial       CP2102 USB to UART
  5    arduino-uno        2341:0043  serial       Arduino Uno

  6    cherry-kbd         046A:0011  keyboard     Cherry Keyboard
  7    logi-mouse         046D:C077  mouse        Logitech USB Optical Mouse

  8    hp-laserjet        03F0:0417  printer      HP LaserJet 1018
  9    hp-deskjet         03F0:4817  printer      HP DeskJet 2700

--- Razer (VID 0x1532) - WU pushes Razer Synapse 3 ---
 10    razer-blackwidow   1532:0225  keyboard     BlackWidow V3
 11    razer-deathadder   1532:0084  mouse        DeathAdder V2
 12    razer-kraken       1532:0527  hid-generic  Kraken V3 HyperSense
 13    razer-viper        1532:00A5  mouse        Viper V2 Pro

--- Logitech (VID 0x046D) - WU pushes Logitech G Hub or Options+ ---
 14    logi-gpro          046D:C099  mouse        G Pro X Superlight 2
 15    logi-g915          046D:C343  keyboard     G915 TKL Keyboard
 16    logi-g502          046D:C101  mouse        G502 X Plus
 17    logi-mxmaster      046D:C08B  mouse        MX Master 3

--- Corsair (VID 0x1B1C) - WU pushes Corsair iCUE ---
 18    corsair-k100       1B1C:1B7E  keyboard     K100 RGB
 19    corsair-scimitar   1B1C:1BAE  mouse        Scimitar Elite RGB
 20    corsair-hs80       1B1C:0A67  hid-generic  HS80 RGB Wireless

--- SteelSeries (VID 0x1038) - WU pushes SteelSeries GG ---
 21    ss-apex            1038:1610  keyboard     Apex Pro
 22    ss-rival           1038:1726  mouse        Rival 650 Wireless
 23    ss-arctis          1038:12C0  hid-generic  Arctis Nova Pro

--- ASUS ROG (VID 0x0B05) - WU pushes Armoury Crate ---
 24    asus-scope         0B05:1866  keyboard     ROG Strix Scope
 25    asus-chakram       0B05:1A18  mouse        ROG Chakram X
 26    asus-delta         0B05:17F8  hid-generic  ROG Delta Headset
```

---

## How Windows driver distribution works

When the device enumerates, Windows builds a Hardware ID stack and queries it
against its local INF store and Windows Update simultaneously:

```
USB\VID_1532&PID_0225&REV_0200   ← most specific (exact model + rev)
USB\VID_1532&PID_0225            ← model match  →  Synapse INF
USB\VID_1532                     ← vendor match
USB\Class_03&SubClass_01&Prot_01 ← generic HID boot keyboard (in-box)
```

Windows walks top-to-bottom.  The in-box HID driver binds at the class level
immediately; the OEM software download is triggered by the VID:PID match above
it and happens in the background.  The software install completes before any
vendor-protocol handshake, which is why enumerate-only is sufficient.

**Enterprise / WSUS machines** typically block Windows Update driver distribution
or route it through an internal catalog - no download occurs.  That divergence is
itself a useful detection signal during assessments.

**Linux / macOS** never auto-download drivers or companion software.  The in-box
HID / usblp / cdc-acm class driver binds from kernel modules matched by modalias.

---

## How runtime class switching works

`apply` calls `usb_identity_set()` → `apply_class()` which rewrites the mutable
config-descriptor buffer (`s_config_buf`) in place.  The `esp_tinyusb` driver is
never reinstalled; it serves the same buffer pointers at each enumeration.  The
host sees the change on the next `tud_disconnect()` / `tud_connect()` cycle
(≈ 300 ms soft re-plug).

---

## Toolchain notes

### flat vs nested tinyusb_config_t (esp_tinyusb v2.0 → v2.2)

`usb_identity.c` uses the **flat** field layout.  If your component uses the
nested `.descriptor` form, change `usb_identity_start()` to:

```c
const tinyusb_config_t tusb_cfg = {
    .descriptor = {
        .device      = &s_device_desc,
        .string      = s_string_desc,
        .string_count = STRING_DESC_COUNT,
        .config      = s_config_buf,
    },
    .external_phy = false,
};
```

### usb_app_driver.c version sensitivity

`usbd_class_driver_t` lives in `device/usbd_pvt.h` (TinyUSB internals).

* **`.deinit` member**: present from TinyUSB 0.16 / esp_tinyusb 2.0.  Remove
  `.deinit = app_driver_deinit` and its implementation if you get a compile error.
* **`usbd_edpt_*` / `tud_control_xfer` signatures**: check the version of
  `usbd_pvt.h` shipped with your component and adjust casts if types mismatch.

---

## Extending

### Making Synapse's device-present check pass

After Synapse installs it polls the device via vendor HID feature reports.
`tud_hid_get_report_cb` currently returns 0 (STALL - harmless for install).  To
survive the Synapse handshake and suppress "device not found":

1. Capture the GET_REPORT / SET_REPORT traffic from a real device with
   Wireshark + USBPcap.
2. Replay the expected bytes from `tud_hid_get_report_cb`.

### Adding Mass Storage (MSC)

Enable `CONFIG_TINYUSB_MSC_ENABLED=y`, add `CFG_TUD_MSC 1` to your tusb_config,
add a `cfg_msc[]` descriptor in `usb_class.c`, and implement the six
`tud_msc_*_cb` callbacks with a RAM disk or spiflash backend.

### Adding a new profile

1. Add a `usb_identity_t` entry to `USB_PROFILES[]` in `profiles.c`.
2. Pick the right class (`keyboard` / `mouse` / `hid-generic` for HID devices;
   `serial` for virtual COM ports; `printer` for printer class).
3. Rebuild.  The new profile appears in `list` immediately.

---

## Legal / ethical note

VID:PID pairs used in presets are public USB-IF-registered identifiers.  This
tool presents the identity at the USB layer only - it does not replicate
proprietary firmware, protocols, or content.  Use on systems you own or have
explicit written permission to test.
