// SPDX-License-Identifier: MIT
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_console.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"
#include "argtable3/argtable3.h"

#include "usb_identity.h"
#include "usb_class.h"
#include "profiles.h"
#include "console_cmds.h"

static const char *TAG = "console";

// Edit buffer. Commands edit this; 'apply' commits it to the USB stack.
static usb_identity_t s_edit;

static void print_identity(const usb_identity_t *id)
{
    printf("  name   : %s\n",     id->name);
    printf("  class  : %s\n",     usb_class_name(id->dev_class));
    printf("  VID    : 0x%04X\n", id->vid);
    printf("  PID    : 0x%04X\n", id->pid);
    printf("  bcdDev : 0x%04X\n", id->bcd_device);
    printf("  mfr    : %s\n",     id->manufacturer);
    printf("  product: %s\n",     id->product);
    printf("  serial : %s\n",     id->serial);
}

// ---- list ----
static int cmd_list(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("Preset profiles:\n");
    for (size_t i = 0; i < USB_PROFILE_COUNT; i++) {
        printf("  [%u] %-14s %04X:%04X  %-11s  %s\n",
               (unsigned)i, USB_PROFILES[i].name,
               USB_PROFILES[i].vid, USB_PROFILES[i].pid,
               usb_class_name(USB_PROFILES[i].dev_class),
               USB_PROFILES[i].product);
    }
    printf("Classes: vendor | keyboard | mouse | hid-generic | serial | printer\n");
    return 0;
}

// ---- use <index> ----
static struct { struct arg_int *index; struct arg_end *end; } use_args;
static int cmd_use(int argc, char **argv)
{
    if (arg_parse(argc, argv, (void **)&use_args) != 0) {
        arg_print_errors(stderr, use_args.end, argv[0]);
        return 1;
    }
    int idx = use_args.index->ival[0];
    if (idx < 0 || (size_t)idx >= USB_PROFILE_COUNT) {
        printf("Index out of range (0..%u)\n", (unsigned)(USB_PROFILE_COUNT - 1));
        return 1;
    }
    s_edit = USB_PROFILES[idx];
    printf("Staged preset '%s' (class=%s). Run 'apply' to activate.\n",
           s_edit.name, usb_class_name(s_edit.dev_class));
    return 0;
}

// ---- set <field> <value> ----
static struct { struct arg_str *field; struct arg_str *value; struct arg_end *end; } set_args;
static int cmd_set(int argc, char **argv)
{
    if (arg_parse(argc, argv, (void **)&set_args) != 0) {
        arg_print_errors(stderr, set_args.end, argv[0]);
        return 1;
    }
    const char *f = set_args.field->sval[0];
    const char *v = set_args.value->sval[0];

    if (!strcmp(f, "vid")) {
        s_edit.vid = (uint16_t)strtol(v, NULL, 16);
    } else if (!strcmp(f, "pid")) {
        s_edit.pid = (uint16_t)strtol(v, NULL, 16);
    } else if (!strcmp(f, "bcddev")) {
        s_edit.bcd_device = (uint16_t)strtol(v, NULL, 16);
    } else if (!strcmp(f, "class")) {
        usb_class_t c;
        if (!usb_class_from_name(v, &c)) {
            printf("Unknown class '%s' "
                   "(vendor|keyboard|mouse|hid-generic|serial|printer)\n", v);
            return 1;
        }
        s_edit.dev_class = c;
    } else if (!strcmp(f, "mfr")) {
        snprintf(s_edit.manufacturer, sizeof(s_edit.manufacturer), "%s", v);
    } else if (!strcmp(f, "prod")) {
        snprintf(s_edit.product, sizeof(s_edit.product), "%s", v);
    } else if (!strcmp(f, "serial")) {
        snprintf(s_edit.serial, sizeof(s_edit.serial), "%s", v);
    } else if (!strcmp(f, "name")) {
        snprintf(s_edit.name, sizeof(s_edit.name), "%s", v);
    } else {
        printf("Unknown field '%s' "
               "(vid|pid|bcddev|class|mfr|prod|serial|name)\n", f);
        return 1;
    }
    printf("Staged %s. Run 'apply' to activate.\n", f);
    return 0;
}

// ---- show ----
static int cmd_show(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("Staged (edit buffer):\n");
    print_identity(&s_edit);
    printf("Live (currently enumerated):\n");
    print_identity(usb_identity_get());
    return 0;
}

// ---- apply ----
static int cmd_apply(int argc, char **argv)
{
    (void)argc; (void)argv;
    usb_identity_set(&s_edit);
    usb_identity_reenumerate();
    printf("Applied. Presenting %04X:%04X as class '%s' (%s).\n",
           s_edit.vid, s_edit.pid, usb_class_name(s_edit.dev_class), s_edit.product);
    return 0;
}

// ---- save / load ----
static int cmd_save(int argc, char **argv)
{
    (void)argc; (void)argv;
    esp_err_t err = usb_identity_nvs_save(&s_edit);
    if (err == ESP_OK) { printf("Saved staged identity to NVS.\n"); return 0; }
    printf("Save failed: %s\n", esp_err_to_name(err));
    return 1;
}

static int cmd_load(int argc, char **argv)
{
    (void)argc; (void)argv;
    esp_err_t err = usb_identity_nvs_load(&s_edit);
    if (err != ESP_OK) { printf("Load failed: %s\n", esp_err_to_name(err)); return 1; }
    printf("Loaded from NVS into edit buffer. Run 'apply' to activate.\n");
    print_identity(&s_edit);
    return 0;
}

// ---- reboot ----
static int cmd_reboot(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("Rebooting...\n");
    esp_restart();
    return 0;
}

void console_register_commands(void)
{
    s_edit = *usb_identity_get();

    const esp_console_cmd_t simple_cmds[] = {
        { .command = "list",   .help = "List preset profiles (with class)",       .func = cmd_list   },
        { .command = "show",   .help = "Show staged + live identity",             .func = cmd_show   },
        { .command = "apply",  .help = "Activate staged identity (re-enumerates)", .func = cmd_apply  },
        { .command = "save",   .help = "Persist staged identity to NVS",          .func = cmd_save   },
        { .command = "load",   .help = "Load identity from NVS into edit buffer",  .func = cmd_load   },
        { .command = "reboot", .help = "Restart the device",                      .func = cmd_reboot },
    };
    for (size_t i = 0; i < sizeof(simple_cmds) / sizeof(simple_cmds[0]); i++) {
        ESP_ERROR_CHECK(esp_console_cmd_register(&simple_cmds[i]));
    }

    use_args.index = arg_int1(NULL, NULL, "<index>", "profile index from 'list'");
    use_args.end   = arg_end(1);
    const esp_console_cmd_t use_cmd = {
        .command = "use", .help = "Load a preset into the edit buffer",
        .func = cmd_use, .argtable = &use_args };
    ESP_ERROR_CHECK(esp_console_cmd_register(&use_cmd));

    set_args.field = arg_str1(NULL, NULL, "<field>", "vid|pid|bcddev|class|mfr|prod|serial|name");
    set_args.value = arg_str1(NULL, NULL, "<value>", "hex for ids, name for class, text for strings");
    set_args.end   = arg_end(2);
    const esp_console_cmd_t set_cmd = {
        .command = "set", .help = "Edit one field of the staged identity",
        .func = cmd_set, .argtable = &set_args };
    ESP_ERROR_CHECK(esp_console_cmd_register(&set_cmd));

    ESP_LOGI(TAG, "Identity console ready");
}
