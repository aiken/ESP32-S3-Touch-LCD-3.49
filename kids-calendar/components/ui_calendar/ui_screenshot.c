#include "ui_calendar.h"
#include "ui_styles.h"

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"

static const char *TAG = "SCREENSHOT";

/* Periodic screenshot task for debugging */
static void screenshot_task(void *arg)
{
    (void)arg;

    ESP_LOGI(TAG, "Screenshot task started, will dump framebuffer every 10 seconds...");

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "=== Auto screenshot ===");
        dump_framebuffer();
        ESP_LOGI(TAG, "=== End auto screenshot ===");
    }
}

/* Take a screenshot and send it via serial */
void take_screenshot(void)
{
    lv_disp_t *disp = lv_disp_get_default();
    if (disp == NULL) {
        ESP_LOGE(TAG, "No display found");
        return;
    }

    /* Force a refresh */
    lv_obj_invalidate(lv_scr_act());
    lv_timer_handler();

    /* Get framebuffer */
    lv_draw_buf_t *draw_buf = lv_display_get_buf_active(disp);
    if (draw_buf == NULL || draw_buf->data == NULL) {
        ESP_LOGE(TAG, "No draw buffer");
        return;
    }

    lv_coord_t hor_res = lv_disp_get_hor_res(disp);
    lv_coord_t ver_res = lv_disp_get_ver_res(disp);

    ESP_LOGI(TAG, "Screenshot: %dx%d, buffer=%p, size=%d",
             hor_res, ver_res, draw_buf->data, draw_buf->data_size);

    /* Send screenshot as hex data */
    ESP_LOGI(TAG, "=== SCREENSHOT START ===");
    ESP_LOGI(TAG, "WIDTH:%d", hor_res);
    ESP_LOGI(TAG, "HEIGHT:%d", ver_res);
    ESP_LOGI(TAG, "FORMAT:RGB565");

    /* Send data in chunks */
    const int chunk_size = 64;
    for (uint32_t i = 0; i < draw_buf->data_size; i += chunk_size) {
        uint32_t len = (i + chunk_size < draw_buf->data_size) ? chunk_size : (draw_buf->data_size - i);
        (void)len;
        ESP_LOGI(TAG, "DATA:%08X:%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
                 i,
                 draw_buf->data[i], draw_buf->data[i+1], draw_buf->data[i+2], draw_buf->data[i+3],
                 draw_buf->data[i+4], draw_buf->data[i+5], draw_buf->data[i+6], draw_buf->data[i+7],
                 draw_buf->data[i+8], draw_buf->data[i+9], draw_buf->data[i+10], draw_buf->data[i+11],
                 draw_buf->data[i+12], draw_buf->data[i+13], draw_buf->data[i+14], draw_buf->data[i+15]);
    }

    ESP_LOGI(TAG, "=== SCREENSHOT END ===");
}

/* Dump framebuffer as ASCII art */
void dump_framebuffer(void)
{
    lv_disp_t *disp = lv_disp_get_default();
    if (disp == NULL) {
        ESP_LOGE(TAG, "No display found");
        return;
    }

    /* Force a refresh */
    lv_obj_invalidate(lv_scr_act());
    lv_timer_handler();

    /* Get framebuffer */
    lv_draw_buf_t *draw_buf = lv_display_get_buf_active(disp);
    if (draw_buf == NULL || draw_buf->data == NULL) {
        ESP_LOGE(TAG, "No draw buffer");
        return;
    }

    lv_coord_t hor_res = lv_disp_get_hor_res(disp);
    ESP_LOGI(TAG, "=== Framebuffer dump (top-left 40x20) ===");
    uint16_t *fb = (uint16_t *)draw_buf->data;
    int dump_w = 40;
    int dump_h = 20;

    for (int y = 0; y < dump_h; y++) {
        char line[128] = {0};
        int pos = 0;
        for (int x = 0; x < dump_w; x++) {
            uint16_t pixel = fb[y * hor_res + x];
            uint8_t r = (pixel >> 11) & 0x1F;
            uint8_t g = (pixel >> 5) & 0x3F;
            uint8_t b = pixel & 0x1F;
            uint8_t gray = (r * 3 + g * 6 + b * 2) / 12;

            char c;
            if (gray > 200) c = '#';
            else if (gray > 150) c = '*';
            else if (gray > 100) c = '+';
            else if (gray > 50) c = '-';
            else c = ' ';

            line[pos++] = c;
        }
        line[pos] = '\0';
        ESP_LOGI(TAG, "%s", line);
    }
    ESP_LOGI(TAG, "=== End dump ===");
}

/* Start the screenshot task */
void start_screenshot_task(void)
{
    xTaskCreatePinnedToCore(screenshot_task, "screenshot", 4096, NULL, 3, NULL, 0);
}
