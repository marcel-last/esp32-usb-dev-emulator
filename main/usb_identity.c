// SPDX-License-Identifier: MIT
#include <stdio.h>
#include <string.h>

#include "usb_identity.h"
#include "usb_class.h"
#include "tinyusb.h"
#include "nvs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "usb_id";

#define NVS_NS   "usbid"
#define NVS_KEY  "active"

// Live identity. The string-descriptor table points at these buffers, so
// editing them in place changes the strings the host reads on next enumeration.
static usb_identity_t s_active;

// Device descriptor. VID/PID/bcdDevice and the class triple are rewritten by
// usb_identity_set(); esp_tinyusb serves this pointer live at each enumeration.
static tusb_desc_device_t s_device_desc = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = 64,
    .idVendor           = 0x303A,
    .idProduct          = 0x4001,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01,
};

// Mutable configuration-descriptor buffer. usb_class_select() rewrites its
// contents per active class; the served pointer never changes, so no driver
// reinstall is needed to switch classes -- just rewrite + re-enumerate.
static uint8_t s_config_buf[128];

// String descriptor table. Index 0 = LANGID; 1..3 point at s_active buffers.
static const char *s_string_desc[] = {
    (const char[]){0x09, 0x04},   // 0: English (US)
    s_active.manufacturer,        // 1: iManufacturer
    s_active.product,             // 2: iProduct
    s_active.serial,              // 3: iSerialNumber
};
enum { STRING_DESC_COUNT = sizeof(s_string_desc) / sizeof(s_string_desc[0]) };

const usb_identity_t *usb_identity_get(void) { return &s_active; }

// Rewrite the config buffer + device-descriptor class codes for the given class.
static void apply_class(usb_class_t cls)
{
    uint8_t dev_codes[3];
    size_t len = usb_class_select(cls, s_config_buf, sizeof(s_config_buf), dev_codes);
    if (len == 0) {
        ESP_LOGE(TAG, "class %d descriptor too large for buffer", (int)cls);
        return;
    }
    s_device_desc.bDeviceClass    = dev_codes[0];
    s_device_desc.bDeviceSubClass = dev_codes[1];
    s_device_desc.bDeviceProtocol = dev_codes[2];
}

void usb_identity_set(const usb_identity_t *id)
{
    s_active.dev_class  = id->dev_class;
    s_active.vid        = id->vid;
    s_active.pid        = id->pid;
    s_active.bcd_device = id->bcd_device;
    snprintf(s_active.name,         sizeof(s_active.name),         "%s", id->name);
    snprintf(s_active.manufacturer, sizeof(s_active.manufacturer), "%s", id->manufacturer);
    snprintf(s_active.product,      sizeof(s_active.product),      "%s", id->product);
    snprintf(s_active.serial,       sizeof(s_active.serial),       "%s", id->serial);

    s_device_desc.idVendor  = s_active.vid;
    s_device_desc.idProduct = s_active.pid;
    s_device_desc.bcdDevice = s_active.bcd_device;

    apply_class(s_active.dev_class);
}

void usb_identity_reenumerate(void)
{
    if (tud_connected()) {
        tud_disconnect();
        vTaskDelay(pdMS_TO_TICKS(300));
        tud_connect();
        ESP_LOGI(TAG, "Re-enumerated as %04X:%04X (%s, class=%s)",
                 s_active.vid, s_active.pid, s_active.product,
                 usb_class_name(s_active.dev_class));
    } else {
        ESP_LOGW(TAG, "Host not connected; new identity shows on next plug-in");
    }
}

esp_err_t usb_identity_start(const usb_identity_t *id)
{
    usb_identity_set(id);   // also fills s_config_buf via apply_class()

    // NOTE: flat tinyusb_config_t fields (esp_tinyusb through v2.2.x). If your
    // component uses the nested `.descriptor` form, see the README.
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor        = &s_device_desc,
        .string_descriptor        = s_string_desc,
        .string_descriptor_count  = STRING_DESC_COUNT,
        .external_phy             = false,
        .configuration_descriptor = s_config_buf,
    };
    return tinyusb_driver_install(&tusb_cfg);
}

esp_err_t usb_identity_nvs_save(const usb_identity_t *id)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(h, NVS_KEY, id, sizeof(*id));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

esp_err_t usb_identity_nvs_load(usb_identity_t *id)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    size_t len = sizeof(*id);
    err = nvs_get_blob(h, NVS_KEY, id, &len);
    nvs_close(h);
    if (err == ESP_OK && len != sizeof(*id)) return ESP_ERR_INVALID_SIZE;
    return err;
}
