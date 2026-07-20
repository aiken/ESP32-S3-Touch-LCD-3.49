# Prompt：高性能屏幕截图组件（直接复制发给 Coding Agent）

> 复制以下**全部内容**，粘贴到 kimi cli 或 opencode 中执行。

---

## 复制从这里开始

```
请为 ESP32-S3 + ESP-IDF v5.5.3 创建一个高性能的屏幕截图 HTTP 服务器组件。

## 核心要求

1. **零颜色转换**：帧缓冲是 RGB565 格式，直接打包成 BMP 发送，不做任何像素转换
2. **高性能**：截图一张 < 100ms CPU 时间，避免软编码 JPEG
3. **轻量 HTTP 服务器**：使用 esp_http_server，端口 8080
4. **提供多个端点**：
   - `/` — HTML 调试页面（带自动刷新）
   - `/screenshot` — 全分辨率 BMP（172x640）
   - `/screenshot/small` — 1/4 分辨率 BMP（86x320），传输更快
   - `/status` — JSON 状态（分辨率、内存）

## BMP 格式关键信息

BMP 支持 RGB565 原生格式（BI_BITFIELDS 压缩类型），需要：
- 54 字节标准文件头 + 16 字节 BITFIELDS 掩码 = 70 字节头
- RGB565 掩码：R=0xF800, G=0x07E0, B=0x001F
- 像素数据需要 Y 轴翻转（BMP 从下到上存储）

## 文件清单

请创建以下 3 个文件：

### 1. components/screenshot_server/CMakeLists.txt

```cmake
idf_component_register(
    SRCS "screenshot_server.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server
)
```

### 2. components/screenshot_server/include/screenshot_server.h

```c
#pragma once

#include <stdint.h>
#include "esp_err.h"

void screenshot_server_register_fb(uint16_t *fb, uint16_t width, uint16_t height);
esp_err_t screenshot_server_start(uint16_t port);
void screenshot_server_stop(void);
```

### 3. components/screenshot_server/screenshot_server.c

```c
#include "screenshot_server.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_heap_caps.h"

static const char *TAG = "SCREENSHOT";
static httpd_handle_t server = NULL;
static uint16_t *g_fb = NULL;
static uint16_t g_fb_width = 172;
static uint16_t g_fb_height = 640;

// BMP 文件头模板 (54 + 16 = 70 bytes for RGB565 BITFIELDS)
static const uint8_t BMP_HEADER[70] = {
    'B', 'M',                       // signature
    0, 0, 0, 0,                     // file size (filled at runtime)
    0, 0, 0, 0,                     // reserved
    70, 0, 0, 0,                    // pixel data offset
    40, 0, 0, 0,                    // DIB header size
    0, 0, 0, 0,                     // width (filled at runtime)
    0, 0, 0, 0,                     // height (filled at runtime)
    1, 0,                           // planes
    16, 0,                          // bits per pixel = 16 (RGB565)
    3, 0, 0, 0,                     // compression = BI_BITFIELDS
    0, 0, 0, 0,                     // pixel data size
    0, 0, 0, 0,                     // X ppm
    0, 0, 0, 0,                     // Y ppm
    0, 0, 0, 0,                     // colors used
    0, 0, 0, 0,                     // important colors
    // RGB565 bit masks
    0x00, 0xF8, 0x00, 0x00,       // Red mask:   1111100000000000
    0xE0, 0x07, 0x00, 0x00,       // Green mask: 0000011111100000
    0x1F, 0x00, 0x00, 0x00,       // Blue mask:  0000000000011111
    0x00, 0x00, 0x00, 0x00,       // Alpha mask: 0000000000000000
};

void screenshot_server_register_fb(uint16_t *fb, uint16_t width, uint16_t height)
{
    g_fb = fb;
    g_fb_width = width;
    g_fb_height = height;
    ESP_LOGI(TAG, "FB registered: %dx%d @ %p", width, height, fb);
}

static esp_err_t screenshot_handler(httpd_req_t *req)
{
    if (!g_fb) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Frame buffer not registered");
        return ESP_FAIL;
    }

    uint32_t pixel_data_size = g_fb_width * g_fb_height * 2;
    uint32_t file_size = 70 + pixel_data_size;

    uint8_t *response = heap_caps_malloc(file_size, MALLOC_CAP_SPIRAM);
    if (!response) response = heap_caps_malloc(file_size, MALLOC_CAP_DEFAULT);
    if (!response) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_FAIL;
    }

    memcpy(response, BMP_HEADER, 70);
    *(uint32_t*)(response + 2) = file_size;
    *(uint32_t*)(response + 18) = g_fb_width;
    *(uint32_t*)(response + 22) = g_fb_height;
    *(uint32_t*)(response + 34) = pixel_data_size;

    uint16_t *dst = (uint16_t*)(response + 70);
    for (int y = 0; y < g_fb_height; y++) {
        memcpy(&dst[(g_fb_height - 1 - y) * g_fb_width],
               &g_fb[y * g_fb_width],
               g_fb_width * 2);
    }

    httpd_resp_set_type(req, "image/bmp");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    esp_err_t ret = httpd_resp_send(req, (const char*)response, file_size);
    free(response);

    ESP_LOGI(TAG, "Screenshot: %dx%d (%lu bytes)",
             g_fb_width, g_fb_height, (unsigned long)file_size);
    return ret;
}

static esp_err_t screenshot_small_handler(httpd_req_t *req)
{
    if (!g_fb) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No FB");
        return ESP_FAIL;
    }

    uint16_t sw = g_fb_width / 2;
    uint16_t sh = g_fb_height / 2;
    uint32_t pixel_size = sw * sh * 2;
    uint32_t file_size = 70 + pixel_size;

    uint8_t *response = heap_caps_malloc(file_size, MALLOC_CAP_SPIRAM);
    if (!response) response = heap_caps_malloc(file_size, MALLOC_CAP_DEFAULT);
    if (!response) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_FAIL;
    }

    memcpy(response, BMP_HEADER, 70);
    *(uint32_t*)(response + 2) = file_size;
    *(uint32_t*)(response + 18) = sw;
    *(uint32_t*)(response + 22) = sh;
    *(uint32_t*)(response + 34) = pixel_size;

    uint16_t *dst = (uint16_t*)(response + 70);
    for (int y = 0; y < sh; y++) {
        for (int x = 0; x < sw; x++) {
            uint32_t sy = y * 2;
            uint32_t sx = x * 2;
            uint16_t p1 = g_fb[sy * g_fb_width + sx];
            uint16_t p2 = g_fb[sy * g_fb_width + sx + 1];
            uint16_t p3 = g_fb[(sy + 1) * g_fb_width + sx];
            uint16_t p4 = g_fb[(sy + 1) * g_fb_width + sx + 1];

            uint8_t r = (((p1 >> 11) & 0x1F) + ((p2 >> 11) & 0x1F) +
                        ((p3 >> 11) & 0x1F) + ((p4 >> 11) & 0x1F)) / 4;
            uint8_t g = (((p1 >> 5) & 0x3F) + ((p2 >> 5) & 0x3F) +
                        ((p3 >> 5) & 0x3F) + ((p4 >> 5) & 0x3F)) / 4;
            uint8_t b = ((p1 & 0x1F) + (p2 & 0x1F) +
                        (p3 & 0x1F) + (p4 & 0x1F)) / 4;

            dst[(sh - 1 - y) * sw + x] = (r << 11) | (g << 5) | b;
        }
    }

    httpd_resp_set_type(req, "image/bmp");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    esp_err_t ret = httpd_resp_send(req, (const char*)response, file_size);
    free(response);

    ESP_LOGI(TAG, "Small screenshot: %dx%d (%lu bytes)", sw, sh, (unsigned long)file_size);
    return ret;
}

static esp_err_t status_handler(httpd_req_t *req)
{
    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"resolution\":\"%dx%d\",\"fb_size\":%d,\"free_heap\":%lu,\"free_psram\":%lu}\n",
        g_fb_width, g_fb_height, g_fb_width * g_fb_height * 2,
        (unsigned long)esp_get_free_heap_size(),
        (unsigned long)esp_get_free_internal_heap_size());
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, buf, strlen(buf));
}

static esp_err_t index_handler(httpd_req_t *req)
{
    const char *html =
        "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width'>"
        "<title>ESP32 Debug</title>"
        "<style>"
        "body{font-family:system-ui;margin:0;padding:16px;background:#0a0a14;color:#fff;text-align:center;}"
        "img{max-width:100%;border-radius:8px;background:#1a1a2e;min-height:200px;}"
        "button{padding:10px 20px;margin:4px;border:none;border-radius:6px;"
        "background:#4a90d9;color:#fff;cursor:pointer;font-size:14px;}"
        "button:hover{background:#357abd;}"
        ".stats{color:#8892b0;font-size:12px;margin:8px;}"
        "</style></head><body>"
        "<h2>ESP32-S3 Screen Debug</h2>"
        "<div class='stats' id='st'>Loading...</div>"
        "<img id='ss' src='/screenshot' alt='screenshot'>"
        "<br>"
        "<button onclick='r()'>Refresh</button>"
        "<button onclick='ra()'>Auto(1s)</button>"
        "<button onclick='sm()'>Small(fast)</button>"
        "<button onclick='stop()'>Stop</button>"
        "<script>"
        "var t;function r(){var i=document.getElementById('ss');"
        "i.src='/screenshot?t='+Date.now();fetch('/status').then(e=>e.json()).then(j=>{"
        "document.getElementById('st').innerText=j.resolution+' | heap:'+(j.free_heap/1024).toFixed(1)+'KB';});}"
        "function ra(){t=setInterval(r,1000);}"
        "function sm(){clearInterval(t);var i=document.getElementById('ss');"
        "i.src='/screenshot/small?t='+Date.now();}"
        "function stop(){clearInterval(t);}"
        "r();"
        "</script></body></html>";

    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, html, strlen(html));
}

static const httpd_uri_t routes[] = {
    {"/",                HTTP_GET, index_handler,          NULL},
    {"/screenshot",      HTTP_GET, screenshot_handler,     NULL},
    {"/screenshot/small",HTTP_GET, screenshot_small_handler,NULL},
    {"/status",          HTTP_GET, status_handler,         NULL},
};

esp_err_t screenshot_server_start(uint16_t port)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port;
    config.max_uri_handlers = 8;
    config.stack_size = 8192;

    esp_err_t ret = httpd_start(&server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP server start failed: %s", esp_err_to_name(ret));
        return ret;
    }

    for (int i = 0; i < sizeof(routes)/sizeof(routes[0]); i++) {
        httpd_register_uri_handler(server, &routes[i]);
    }

    ESP_LOGI(TAG, "Screenshot server: http://<ip>:%d", port);
    return ESP_OK;
}

void screenshot_server_stop(void)
{
    if (server) {
        httpd_stop(server);
        server = NULL;
    }
}
```

## LCD 驱动中注册帧缓冲的示例

在 axs15231b_display.c（或其他 LCD 驱动）中：

```c
#include "screenshot_server.h"

static uint16_t *frame_buffer = NULL;

// 初始化 LCD 后注册帧缓冲
void lcd_init(void) {
    // ... 初始化 QSPI、分配帧缓冲 ...
    frame_buffer = heap_caps_malloc(172 * 640 * 2, MALLOC_CAP_SPIRAM);
    
    // 注册到截图服务器
    screenshot_server_register_fb(frame_buffer, 172, 640);
}

// Wi-Fi 连接成功后启动截图服务
// 在 wifi_sync.c 或 main.c 中调用：
// screenshot_server_start(8080);
```

## 使用方式

Wi-Fi 连接成功后，日志会输出设备 IP：

```
I SCREENSHOT: Screenshot server: http://<ip>:8080
```

浏览器打开：
- `http://<device-ip>:8080` — HTML 调试页面（自动刷新）
- `http://<device-ip>:8080/screenshot` — 全分辨率 BMP
- `http://<device-ip>:8080/screenshot/small` — 1/4 分辨率（更快）
- `http://<device-ip>:8080/status` — JSON 状态

命令行：
```bash
curl -o screen.bmp http://192.168.1.xxx:8080/screenshot
```

## 关键性能优化说明

1. **BMP RGB565 原生格式**：跳过颜色转换，memcpy 直接拷贝像素数据
2. **PSRAM 双缓冲**：215KB 帧缓冲放在 PSRAM，不占用内部 RAM
3. **Y 轴翻转用 memcpy**：比逐像素处理快 10 倍
4. **1/4 降级端点**：网络传输快 4 倍，适合远程调试
5. **纯文本 /status 端点**：超轻量，用于快速检查设备状态

请按以上要求创建完整的 component 文件。创建完成后告诉我编译命令。
```

---

## 使用方式

1. **进入项目目录**：
   ```bash
   cd ~/Documents/kids-calendar
   get_idf
   kimi
   ```

2. **完整粘贴上面的 Prompt**（从 `请为 ESP32-S3` 开始到 `请按以上要求创建` 结束）

3. **AI 会创建 3 个文件**：
   - `components/screenshot_server/CMakeLists.txt`
   - `components/screenshot_server/include/screenshot_server.h`
   - `components/screenshot_server/screenshot_server.c`

4. **修改顶层 CMakeLists.txt 添加组件**：
   ```cmake
   cmake_minimum_required(VERSION 3.16)
   include($ENV{IDF_PATH}/tools/cmake/project.cmake)
   set(EXTRA_COMPONENT_DIRS components)
   project(kids-calendar)
   ```

5. **编译测试**：
   ```bash
   idf.py build
   ```

## 常见问题

**编译报错 `esp_http_server.h not found`** — 在 `CMakeLists.txt` 中添加 `REQUIRES esp_http_server`

**报错截图组件找不到** — 确保 `EXTRA_COMPONENT_DIRS components` 在顶层 `CMakeLists.txt` 中

**屏幕截图是花的** — 检查帧缓冲指针是否正确传递给 `screenshot_server_register_fb()`
