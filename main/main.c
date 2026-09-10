// SPDX-License-Identifier: MIT
//
// ESP32-S3 USB identity emulator.
//   * Native USB port (GPIO19/20)  -> emulated device shown to the host under test
//   * UART port (bridge chip)       -> serial console to pick/edit the identity
//
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_console.h"

#include "usb_identity.h"
#include "profiles.h"
#include "console_cmds.h"

static const char *TAG = "main";

void app_main(void)
{
    // --- NVS (stores the persisted identity) ---
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // --- Pick the boot identity: saved one if present, else first preset ---
    usb_identity_t boot_id;
    if (usb_identity_nvs_load(&boot_id) == ESP_OK) {
        ESP_LOGI(TAG, "Loaded saved identity '%s' (%04X:%04X)",
                 boot_id.name, boot_id.vid, boot_id.pid);
    } else {
        boot_id = USB_PROFILES[0];
        ESP_LOGI(TAG, "No saved identity; using preset '%s'", boot_id.name);
    }

    // --- Bring up the USB device stack with that identity ---
    ESP_ERROR_CHECK(usb_identity_start(&boot_id));
    ESP_LOGI(TAG, "USB up as %04X:%04X (%s)",
             boot_id.vid, boot_id.pid, boot_id.product);

    // --- Start the config console on UART0 (the DevKitC "UART" port) ---
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_cfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_cfg.prompt = "usbid> ";
    repl_cfg.max_cmdline_length = 128;

    esp_console_dev_uart_config_t uart_cfg = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_cfg, &repl_cfg, &repl));

    esp_console_register_help_command();
    console_register_commands();

    printf("\nESP32-S3 USB identity emulator ready. Type 'help'.\n");
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
