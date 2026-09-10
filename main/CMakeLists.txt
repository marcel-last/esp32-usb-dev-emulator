idf_component_register(
    SRCS
        "main.c"
        "usb_identity.c"
        "usb_class.c"
        "usb_app_driver.c"
        "profiles.c"
        "console_cmds.c"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        console
        esp_tinyusb
)
