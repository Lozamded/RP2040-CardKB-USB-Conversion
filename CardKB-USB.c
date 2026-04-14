#include "bsp/board_api.h"
#include "class/hid/hid.h"
#include "tusb.h"
#include "usb_descriptors.h"

#include "hardware/i2c.h"
#include "pico/error.h"
#include "pico/stdlib.h"

#define I2C_PORT i2c0
#define SDA_PIN 4 //Amarillo
#define SCL_PIN 5 //Blanco
//Rojo a 3v3
//Negro a GND
#define CARDKB_ADDR 0x5F
#define I2C_TIMEOUT_US 5000u

/* Tabla ASCII → (shift, keycode) de TinyUSB; teclado US */
static const uint8_t k_ascii_to_hid[128][2] = {HID_ASCII_TO_KEYCODE};

static bool cardkb_byte_to_hid(uint8_t c, uint8_t *modifier_out, uint8_t *key_out) {
    *modifier_out = 0;

    //Flechas
    switch (c) {
    case 0xB4:
        *key_out = HID_KEY_ARROW_LEFT;
        return true;
    case 0xB5:
        *key_out = HID_KEY_ARROW_UP;
        return true;
    case 0xB6:
        *key_out = HID_KEY_ARROW_DOWN;
        return true;
    case 0xB7:
        *key_out = HID_KEY_ARROW_RIGHT;
        return true;
    /* Variantes que algunas unidades / firmwares envían */
    case 0x0A:
        *key_out = HID_KEY_ARROW_DOWN;
        return true;
    case 0x0B:
        *key_out = HID_KEY_ARROW_UP;
        return true;
    case 0x1C:
        *key_out = HID_KEY_ARROW_LEFT;
        return true;
    case 0x1D:
        *key_out = HID_KEY_ARROW_RIGHT;
        return true;
    default:
        break;
    }

    if (c >= 128u) {
        return false;
    }

    uint8_t sh = k_ascii_to_hid[c][0];
    uint8_t kc = k_ascii_to_hid[c][1];
    *modifier_out = sh ? KEYBOARD_MODIFIER_LEFTSHIFT : 0;
    *key_out = kc;
    return kc != 0;
}

static void hid_key_stroke(uint8_t modifier, uint8_t keycode) {
    if (!tud_hid_ready()) {
        return;
    }
    uint8_t keys[6] = {0};
    keys[0] = keycode;
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, modifier, keys);

    for (int i = 0; i < 400; i++) {
        tud_task();
        sleep_us(50);
    }

    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
    for (int i = 0; i < 100; i++) {
        tud_task();
        sleep_us(20);
    }
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer,
                               uint16_t reqlen) {
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer,
                           uint16_t bufsize) {
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}

int main(void) {
    board_init();

    tusb_rhport_init_t dev_init = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_AUTO,
    };
    tusb_init(BOARD_TUD_RHPORT, &dev_init);

    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);

    uint8_t key = 0;
    bool ya = false;

    while (true) {
        tud_task();

        ya = false;
        for (int burst = 0; burst < 16; burst++) {
            int result = i2c_read_timeout_us(I2C_PORT, CARDKB_ADDR, &key, 1, false, I2C_TIMEOUT_US);
            if (result != 1 || key == 0 || ya) {
                continue;
            }
            uint8_t mod = 0;
            uint8_t kc = 0;
            if (cardkb_byte_to_hid(key, &mod, &kc)) {
                hid_key_stroke(mod, kc);
            }
            ya = true;
        }

        sleep_ms(1);
    }
}
