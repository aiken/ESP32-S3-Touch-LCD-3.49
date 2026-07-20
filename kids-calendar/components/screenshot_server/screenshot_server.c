#include "screenshot_server.h"

#include <string.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_heap_caps.h"

static const char *TAG = "SCREENSHOT";
static httpd_handle_t s_server = NULL;
static uint16_t *s_fb = NULL;
static uint16_t s_w = 172, s_h = 640;

static const uint8_t BMP_HDR[70] = {
    'B','M', 0,0,0,0, 0,0,0,0, 70,0,0,0,
    40,0,0,0, 0,0,0,0, 0,0,0,0, 1,0, 16,0, 3,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0x00,0xF8,0x00,0x00, 0xE0,0x07,0x00,0x00,
    0x1F,0x00,0x00,0x00, 0x00,0x00,0x00,0x00
};

static esp_err_t screen_handler(httpd_req_t *req)
{
    if (!s_fb) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No FB");
    }

    uint32_t pix = s_w * s_h * 2;
    uint32_t file = 70 + pix;
    uint8_t *rsp = heap_caps_malloc(file, MALLOC_CAP_SPIRAM);
    if (!rsp) {
        rsp = heap_caps_malloc(file, MALLOC_CAP_DEFAULT);
    }
    if (!rsp) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
    }

    memcpy(rsp, BMP_HDR, 70);
    *(uint32_t*)(rsp + 2) = file;
    *(uint32_t*)(rsp + 18) = s_w;
    *(uint32_t*)(rsp + 22) = s_h;
    *(uint32_t*)(rsp + 34) = pix;

    uint16_t *dst = (uint16_t*)(rsp + 70);
    for (int y = 0; y < s_h; y++) {
        memcpy(&dst[(s_h - 1 - y) * s_w], &s_fb[y * s_w], s_w * 2);
    }

    httpd_resp_set_type(req, "image/bmp");
    esp_err_t ret = httpd_resp_send(req, (const char*)rsp, file);
    free(rsp);
    return ret;
}

static const httpd_uri_t uri = {"/debug/screen.bmp", HTTP_GET, screen_handler, NULL};

void screenshot_server_register_fb(uint16_t *fb, uint16_t w, uint16_t h)
{
    s_fb = fb;
    s_w = w;
    s_h = h;
    ESP_LOGI(TAG, "Frame buffer registered: %dx%d", w, h);
}

esp_err_t screenshot_server_start(uint16_t port)
{
    httpd_config_t c = HTTPD_DEFAULT_CONFIG();
    c.server_port = port;
    c.stack_size = 4096;

    esp_err_t r = httpd_start(&s_server, &c);
    if (r) {
        ESP_LOGE(TAG, "Failed to start server: %s", esp_err_to_name(r));
        return r;
    }

    httpd_register_uri_handler(s_server, &uri);
    ESP_LOGI(TAG, "Screenshot server started on port %d", port);
    ESP_LOGI(TAG, "Access: http://<ip>:%d/debug/screen.bmp", port);
    return ESP_OK;
}
