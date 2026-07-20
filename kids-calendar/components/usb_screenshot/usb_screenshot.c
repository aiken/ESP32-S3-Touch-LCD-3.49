#include "usb_screenshot.h"

#include <string.h>
#include "esp_log.h"
#include "driver/usb_serial_jtag.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "USB_SS";
static uint16_t *s_fb = NULL;
static uint16_t s_w = 172, s_h = 640;

static usb_ss_lock_cb s_lock_cb = NULL;
static usb_ss_unlock_cb s_unlock_cb = NULL;
static usb_ss_refresh_cb s_refresh_cb = NULL;

void usb_screenshot_set_hooks(usb_ss_lock_cb lock, usb_ss_unlock_cb unlock,
                              usb_ss_refresh_cb refresh)
{
    s_lock_cb = lock;
    s_unlock_cb = unlock;
    s_refresh_cb = refresh;
}

void usb_screenshot_register_fb(uint16_t *fb, uint16_t w, uint16_t h)
{
    s_fb = fb;
    s_w = w;
    s_h = h;
    ESP_LOGI(TAG, "FB: %dx%d", w, h);
}

void usb_screenshot_send(void)
{
    if (!s_fb) {
        ESP_LOGE(TAG, "No FB");
        return;
    }

    uint32_t pix_size = s_w * s_h * 2;
    uint32_t total = 8 + 8 + pix_size + 6;

    /* Allocate buffer in PSRAM */
    uint8_t *buf = heap_caps_malloc(total, MALLOC_CAP_SPIRAM);
    if (!buf) {
        buf = heap_caps_malloc(total, MALLOC_CAP_DEFAULT);
    }
    if (!buf) {
        ESP_LOGE(TAG, "OOM");
        return;
    }

    /* Grab a consistent frame: hold the LVGL mutex and force a refresh so
       the buffer contains one complete, already byte-swapped generation
       (no tearing / mixed endianness). */
    bool locked = s_lock_cb ? s_lock_cb(-1) : false;
    if (s_refresh_cb) {
        s_refresh_cb();
    }

    uint8_t *p = buf;

    /* Frame header */
    memcpy(p, "FB_START", 8);
    p += 8;

    /* Metadata (little-endian) */
    *(uint16_t*)p = s_w;
    p += 2;
    *(uint16_t*)p = s_h;
    p += 2;
    *(uint32_t*)p = pix_size;
    p += 4;

    /* Pixel data (direct copy, PC handles byte order) */
    memcpy(p, s_fb, pix_size);
    p += pix_size;

    /* Frame tail */
    memcpy(p, "FB_END", 6);

    if (locked && s_unlock_cb) {
        s_unlock_cb();
    }

    /* Send via USB-Serial-JTAG (native USB, /dev/tty.usbmodem*).
       The USJ TX ring buffer is tiny (256 bytes with the default/console
       config), and xRingbufferSend fails immediately for items larger than
       the buffer, so the frame must be written in small chunks. */
    size_t written = 0;
    int zero_writes = 0;
    while (written < total) {
        size_t chunk = total - written;
        if (chunk > 256) {
            chunk = 256;
        }
        int w = usb_serial_jtag_write_bytes((const char *)buf + written,
                                            chunk, pdMS_TO_TICKS(1000));
        if (w < 0) {
            ESP_LOGE(TAG, "USJ write failed");
            break;
        }
        if (w == 0 && ++zero_writes > 5) {
            ESP_LOGE(TAG, "Host not reading, abort at %u/%lu bytes",
                     (unsigned)written, (unsigned long)total);
            break;
        }
        written += w;
    }

    free(buf);
    ESP_LOGI(TAG, "Sent: %u bytes", (unsigned)written);
}

/* Line-based command RX: "screenshot" triggers one frame */
static void cmd_rx_task(void *arg)
{
    char line[32];
    size_t pos = 0;
    uint8_t rx[64];

    for (;;) {
        int n = usb_serial_jtag_read_bytes(rx, sizeof(rx), pdMS_TO_TICKS(50));
        if (n <= 0) {
            vTaskDelay(pdMS_TO_TICKS(10)); /* safety: never busy-spin */
            continue;
        }
        ESP_LOGI(TAG, "RX %d bytes: %.*s", n, n, (char *)rx);
        for (int i = 0; i < n; i++) {
            char c = (char)rx[i];
            if (c == '\n' || c == '\r') {
                line[pos] = '\0';
                if (pos > 0 && strcmp(line, "screenshot") == 0) {
                    usb_screenshot_send();
                }
                pos = 0;
            } else if (pos < sizeof(line) - 1) {
                line[pos++] = c;
            } else {
                pos = 0; /* overflow, reset */
            }
        }
    }
}

void usb_screenshot_start_cmd(void)
{
    /* The USB-Serial-JTAG driver is already installed by the secondary
       console (CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG=y). Install it
       ourselves only if that did not happen. */
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    esp_err_t err = usb_serial_jtag_driver_install(&cfg);
    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGI(TAG, "USJ driver already installed (console)");
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "USJ driver install failed: %s", esp_err_to_name(err));
        return;
    }

    xTaskCreatePinnedToCore(cmd_rx_task, "usb_ss_cmd", 4096, NULL, 3, NULL, 0);
    ESP_LOGI(TAG, "Command mode: send 'screenshot' to trigger");
}
