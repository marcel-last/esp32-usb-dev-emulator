// SPDX-License-Identifier: MIT
//
// Minimal application class driver, registered with TinyUSB via the weak
// usbd_app_driver_get_cb() hook. It claims two interface classes that no
// built-in TinyUSB driver handles:
//
//   * 0x07 (Printer)          -- opens the bulk endpoints, drains print data,
//                                and answers the printer class control requests
//                                (GET_DEVICE_ID / GET_PORT_STATUS / SOFT_RESET).
//   * 0xFF (Vendor identity)  -- zero-endpoint interface; claiming it lets
//                                SET_CONFIGURATION succeed for identity-only
//                                profiles.
//
// This is the most version-sensitive file in the project: usbd_class_driver_t
// and the usbd_edpt_* / tud_control_xfer signatures live in TinyUSB's device
// internals and have shifted slightly across releases. If the build complains
// here, see the README "Toolchain notes" -- the fixes are small (usually the
// .deinit member, or an include path).
//
#include <stdio.h>
#include <string.h>
#include "tusb.h"
#include "device/usbd_pvt.h"   // usbd_class_driver_t, usbd_edpt_open/xfer

#include "usb_identity.h"

// Printer class-specific requests (USB Printer Class spec, section 4.2).
#define PRINTER_REQ_GET_DEVICE_ID    0x00
#define PRINTER_REQ_GET_PORT_STATUS  0x01
#define PRINTER_REQ_SOFT_RESET       0x02

static uint8_t s_out_ep = 0;          // printer bulk OUT endpoint address
static uint8_t s_in_ep  = 0;          // printer bulk IN endpoint address
static uint8_t s_rx[64];              // scratch buffer to drain incoming print data
static char    s_devid[160];          // IEEE-1284 device ID (with 2-byte length prefix)

// Build an IEEE-1284 device ID string from the live identity. Format:
//   [len_hi][len_lo] "MFG:<mfr>;MDL:<product>;CMD:...;CLS:PRINTER;"
// where the 2-byte big-endian length INCLUDES the two prefix bytes. Windows and
// CUPS parse MFG/MDL to choose the specific printer model/driver.
static uint16_t build_1284_id(void)
{
    const usb_identity_t *id = usb_identity_get();
    int body = snprintf(s_devid + 2, sizeof(s_devid) - 2,
                        "MFG:%s;MDL:%s;CMD:ESCPL2,BDC;CLS:PRINTER;DES:%s;",
                        id->manufacturer, id->product, id->product);
    if (body < 0) body = 0;
    uint16_t total = (uint16_t)(body + 2);
    s_devid[0] = (uint8_t)(total >> 8);
    s_devid[1] = (uint8_t)(total & 0xFF);
    return total;
}

static void app_driver_init(void) { }

static bool app_driver_deinit(void) { return true; }   // remove on older TinyUSB

static void app_driver_reset(uint8_t rhport)
{
    (void)rhport;
    s_out_ep = 0;
    s_in_ep  = 0;
}

// Claim printer (0x07) and vendor (0xFF) interfaces; open any endpoints.
static uint16_t app_driver_open(uint8_t rhport, tusb_desc_interface_t const *itf, uint16_t max_len)
{
    TU_VERIFY(itf->bInterfaceClass == 0x07 || itf->bInterfaceClass == 0xFF, 0);

    uint16_t drv_len = (uint16_t)(sizeof(tusb_desc_interface_t)
                                  + itf->bNumEndpoints * sizeof(tusb_desc_endpoint_t));
    TU_VERIFY(max_len >= drv_len, 0);

    uint8_t const *p = (uint8_t const *)itf + sizeof(tusb_desc_interface_t);
    for (uint8_t i = 0; i < itf->bNumEndpoints; i++) {
        tusb_desc_endpoint_t const *ep = (tusb_desc_endpoint_t const *)p;
        TU_ASSERT(usbd_edpt_open(rhport, ep));
        if (tu_edpt_dir(ep->bEndpointAddress) == TUSB_DIR_OUT) {
            s_out_ep = ep->bEndpointAddress;
            usbd_edpt_xfer(rhport, s_out_ep, s_rx, sizeof(s_rx));  // arm OUT
        } else {
            s_in_ep = ep->bEndpointAddress;
        }
        p += sizeof(tusb_desc_endpoint_t);
    }
    return drv_len;
}

// Answer the printer class control requests. Anything else -> stall (false).
static bool app_driver_control_xfer(uint8_t rhport, uint8_t stage,
                                    tusb_control_request_t const *req)
{
    if (stage != CONTROL_STAGE_SETUP) {
        return true;  // nothing to do on DATA/ACK stages
    }
    if (req->bmRequestType_bit.type != TUSB_REQ_TYPE_CLASS) {
        return false;
    }
    switch (req->bRequest) {
        case PRINTER_REQ_GET_DEVICE_ID: {
            uint16_t len = build_1284_id();
            uint16_t wlen = req->wLength < len ? req->wLength : len;
            return tud_control_xfer(rhport, req, s_devid, wlen);
        }
        case PRINTER_REQ_GET_PORT_STATUS: {
            static uint8_t status = 0x18;  // bit4 Select, bit3 NotError -> ready
            return tud_control_xfer(rhport, req, &status, 1);
        }
        case PRINTER_REQ_SOFT_RESET:
            return tud_control_xfer(rhport, req, NULL, 0);
        default:
            return false;
    }
}

// Keep the OUT endpoint armed so the host's print data is accepted (and dropped)
// instead of stalling the pipe. We are emulating presence, not printing.
static bool app_driver_xfer(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred)
{
    (void)result; (void)xferred;
    if (ep_addr == s_out_ep && s_out_ep != 0) {
        usbd_edpt_xfer(rhport, s_out_ep, s_rx, sizeof(s_rx));  // re-arm
    }
    return true;
}

static const usbd_class_driver_t s_app_driver = {
#if CFG_TUSB_DEBUG >= 2
    .name            = "APP-PRN-VND",
#endif
    .init            = app_driver_init,
    .deinit          = app_driver_deinit,   // remove this line on TinyUSB < 0.16
    .reset           = app_driver_reset,
    .open            = app_driver_open,
    .control_xfer_cb = app_driver_control_xfer,
    .xfer_cb         = app_driver_xfer,
    .sof             = NULL,
};

// TinyUSB weak hook: return our extra driver(s) alongside the built-in ones.
usbd_class_driver_t const *usbd_app_driver_get_cb(uint8_t *driver_count)
{
    *driver_count = 1;
    return &s_app_driver;
}
