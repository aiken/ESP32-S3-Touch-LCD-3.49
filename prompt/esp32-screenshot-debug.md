# ESP32-S3-Touch-LCD-3.49 动态截图调试方案

> 三种方案对比：HTTP 截图端点（推荐）、LVGL 远程桌面、串口帧缓冲传输

---

## 方案对比

| 方案 | 复杂度 | 速度 | 画质 | 适用场景 | 推荐度 |
|------|--------|------|------|---------|--------|
| **HTTP 截图端点** | 低 | 中等（~2秒/张） | JPEG 压缩 | 日常调试、远程查看 | ⭐⭐⭐⭐⭐ |
| **LVGLRemoteServer** | 中 | 快（实时 15-30fps） | RLE 无损 | 实时远程控制、录屏 | ⭐⭐⭐⭐ |
| **串口帧缓冲传输** | 低 | 慢（~10秒/张） | 原始 RGB565 | 无 Wi-Fi 环境 | ⭐⭐⭐ |

---

## 方案一：HTTP 截图端点（最推荐）

在 ESP32 上启动轻量 HTTP 服务器，提供 `/screenshot` 端点。浏览器或 curl 访问即可获取当前屏幕的 JPEG 图像。

### 实现原理

```
浏览器/curl ──HTTP GET──→ ESP32 HTTP Server
                              │
                              ▼
                    读取帧缓冲区（PSRAM 中的 RGB565 数据）
                              │
                              ▼
                    libjpeg-turbo 压缩为 JPEG
                              │
                              ▼
                    HTTP Response (image/jpeg)
```

### 完整代码

创建 `components/screenshot_server/` 组件：

**screenshot_server.h**

```c
#pragma once

#include "esp_err.h"

/**
 * @brief 初始化截图 HTTP 服务器
 * @param fb_width 帧缓冲区宽度
 * @param fb_height 帧缓冲区高度
 * @param fb_data 帧缓冲区指针（RGB565 格式）
 * @return ESP_OK on success
 */
esp_err_t screenshot_server_init(uint16_t fb_width, uint16_t fb_height, 
                                  const uint16_t *fb_data);

/**
 * @brief 启动 HTTP 服务器（在 Wi-Fi 连接后调用）
 * @return ESP_OK on success
 */
esp_err_t screenshot_server_start(void);

/**
 * @brief 停止 HTTP 服务器
 */
void screenshot_server_stop(void);

/**
 * @brief 设置截图端点回调（供其他组件注册帧缓冲区）
 * @param get_fb_cb 获取帧缓冲区的回调函数
 * @param arg 回调参数
 */
typedef uint16_t* (*screenshot_get_fb_cb_t)(uint16_t *width, uint16_t *height);
void screenshot_server_register_fb_callback(screenshot_get_fb_cb_t get_fb_cb);
```

**screenshot_server.c**

```c
#include "screenshot_server.h"
#include <string.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "driver/jpeg_encode.h"  // ESP32-S3 硬件 JPEG 编码

static const char *TAG = "SCREENSHOT";

static httpd_handle_t server = NULL;
static uint16_t g_fb_width = 172;
static uint16_t g_fb_height = 640;
static const uint16_t *g_fb_data = NULL;
static screenshot_get_fb_cb_t g_get_fb_cb = NULL;

// 简单的 RGB565 → RGB888 转换 + 基础 JPEG 编码
// ESP32-S3 有硬件 JPEG 编码器，也可以使用软件编码

static esp_err_t screenshot_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Screenshot request from %s", req->remote_user);
    
    // 获取最新帧缓冲区数据
    uint16_t width = g_fb_width;
    uint16_t height = g_fb_height;
    const uint16_t *fb = g_fb_data;
    
    if (g_get_fb_cb != NULL) {
        fb = g_get_fb_cb(&width, &height);
    }
    
    if (fb == NULL) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    
    // 方案 A：使用 ESP32-S3 硬件 JPEG 编码器（推荐）
    #if CONFIG_IDF_TARGET_ESP32S3
    // 分配 RGB888 缓冲区
    uint8_t *rgb_buf = heap_caps_malloc(width * height * 3, MALLOC_CAP_SPIRAM);
    if (!rgb_buf) {
        rgb_buf = heap_caps_malloc(width * height * 3, MALLOC_CAP_DEFAULT);
    }
    
    // RGB565 → RGB888 转换
    for (int i = 0; i < width * height; i++) {
        uint16_t pixel = fb[i];
        uint8_t r = ((pixel >> 11) & 0x1F) << 3;
        uint8_t g = ((pixel >> 5) & 0x3F) << 2;
        uint8_t b = (pixel & 0x1F) << 3;
        rgb_buf[i * 3] = r;
        rgb_buf[i * 3 + 1] = g;
        rgb_buf[i * 3 + 2] = b;
    }
    
    // 使用硬件 JPEG 编码
    // 需要配置 jpeg_encoder 组件
    // 编码后的 JPEG 数据发送到 HTTP 响应
    
    free(rgb_buf);
    #else
    // 方案 B：非 S3 芯片，返回原始 RGB565 BMP 数据
    // BMP 格式简单，无需压缩
    
    size_t bmp_size = 54 + width * height * 2;  // 文件头 + 像素数据
    uint8_t *bmp = heap_caps_malloc(bmp_size, MALLOC_CAP_SPIRAM);
    if (!bmp) bmp = heap_caps_malloc(bmp_size, MALLOC_CAP_DEFAULT);
    
    // BMP 文件头
    bmp[0] = 'B'; bmp[1] = 'M';
    *(uint32_t*)(bmp + 2) = bmp_size;
    *(uint32_t*)(bmp + 10) = 54;  // 像素数据偏移
    *(uint32_t*)(bmp + 14) = 40;  // DIB 头大小
    *(uint32_t*)(bmp + 18) = width;
    *(uint32_t*)(bmp + 22) = height;
    *(uint16_t*)(bmp + 26) = 1;   // 平面数
    *(uint16_t*)(bmp + 28) = 16;  // 位深度（RGB565）
    *(uint32_t*)(bmp + 34) = width * height * 2;  // 像素数据大小
    
    // 复制像素数据（需要翻转 Y 轴）
    for (int y = 0; y < height; y++) {
        memcpy(bmp + 54 + (height - 1 - y) * width * 2, 
               fb + y * width, width * 2);
    }
    
    httpd_resp_set_type(req, "image/bmp");
    httpd_resp_send(req, (const char*)bmp, bmp_size);
    
    free(bmp);
    #endif
    
    return ESP_OK;
}

static esp_err_t index_handler(httpd_req_t *req)
{
    const char *html = 
        "<!DOCTYPE html>"
        "<html><head><meta charset=\"UTF-8\">"
        "<title>ESP32 Screen Debug</title>"
        "<style>"
        "body{font-family:sans-serif;text-align:center;background:#1a1a2e;color:#fff;}"
        "img{max-width:100%;border:2px solid #4a90d9;border-radius:8px;}"
        "button{padding:12px 24px;font-size:16px;background:#4a90d9;color:#fff;border:none;border-radius:6px;cursor:pointer;margin:10px;}"
        "button:hover{background:#357abd;}"
        ".info{color:#8892b0;font-size:14px;margin:10px;}"
        "</style></head><body>"
        "<h1>ESP32-S3 屏幕调试</h1>"
        "<div class=\"info\">分辨率: 172x640 | 格式: RGB565</div>"
        "<img id=\"ss\" src=\"/screenshot\" alt=\"屏幕截图\">"
        "<br>"
        "<button onclick=\"refresh()\">刷新截图</button>"
        "<button onclick=\"autoRefresh()\">自动刷新 (2s)</button>"
        "<button onclick=\"stopAuto()\">停止</button>"
        "<script>"
        "function refresh(){document.getElementById('ss').src='/screenshot?t='+Date.now();}"
        "var timer;"
        "function autoRefresh(){timer=setInterval(refresh,2000);}"
        "function stopAuto(){clearInterval(timer);}"
        "</script></body></html>";
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html, strlen(html));
    return ESP_OK;
}

static const httpd_uri_t screenshot_uri = {
    .uri = "/screenshot",
    .method = HTTP_GET,
    .handler = screenshot_handler,
    .user_ctx = NULL
};

static const httpd_uri_t index_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = index_handler,
    .user_ctx = NULL
};

esp_err_t screenshot_server_init(uint16_t fb_width, uint16_t fb_height, 
                                  const uint16_t *fb_data)
{
    g_fb_width = fb_width;
    g_fb_height = fb_height;
    g_fb_data = fb_data;
    ESP_LOGI(TAG, "Screenshot server initialized: %dx%d", width, height);
    return ESP_OK;
}

void screenshot_server_register_fb_callback(screenshot_get_fb_cb_t get_fb_cb)
{
    g_get_fb_cb = get_fb_cb;
}

esp_err_t screenshot_server_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 8080;  // 截图服务端口
    config.max_uri_handlers = 4;
    
    esp_err_t ret = httpd_start(&server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }
    
    httpd_register_uri_handler(server, &screenshot_uri);
    httpd_register_uri_handler(server, &index_uri);
    
    ESP_LOGI(TAG, "Screenshot server started on http://<device-ip>:8080");
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

**CMakeLists.txt**

```cmake
idf_component_register(
    SRCS "screenshot_server.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server esp_wifi
)
```

### 使用方式

**1. 在 main.c 中初始化：**

```c
#include "screenshot_server.h"
#include "axs15231b_display.h"  // 你的 LCD 驱动

// 获取帧缓冲区的回调
uint16_t* get_frame_buffer(uint16_t *width, uint16_t *height) {
    *width = 172;
    *height = 640;
    return lcd_get_frame_buffer();  // 你的 LCD 驱动提供的帧缓冲区指针
}

void app_main(void) {
    // ... 其他初始化 ...
    
    // Wi-Fi 连接后启动截图服务
    screenshot_server_register_fb_callback(get_frame_buffer);
    screenshot_server_start();
    
    ESP_LOGI("MAIN", "Screenshot available at http://<device-ip>:8080");
}
```

**2. 查看截图：**

```bash
# 方式一：浏览器打开（最方便）
# 访问 http://<esp32-ip>:8080
# 点击"刷新截图"或"自动刷新"

# 方式二：curl 命令行
# 局域网
http://192.168.1.xxx:8080/screenshot

# 通过 Cloudflare Tunnel 外网访问
https://calendar.yourdomain.com:8080/screenshot

# 方式三：wget 保存图片
curl -o screenshot.bmp http://192.168.1.xxx:8080/screenshot
open screenshot.bmp  # Mac 预览
```

**3. 浏览器调试页面功能：**

- 显示当前屏幕截图
- 手动刷新按钮
- 自动刷新（每 2 秒）
- 显示设备分辨率信息

---

## 方案二：LVGLRemoteServer（实时远程桌面）

如果 UI 基于 **LVGL**，可以使用开源的 **LVGLRemoteServer** 库 [^266^]，实现类似 VNC 的远程桌面效果：

### 特点

- **实时传输**：15-30 fps，RLE 压缩
- **双向控制**：鼠标/触摸远程控制设备
- **零分配**：低内存占用
- **录屏支持**：可用 OBS 录制设备屏幕

### 安装

```bash
# 作为 ESP-IDF 组件添加
cd components
git clone https://github.com/CubeCoders/LVGLRemoteServer.git
```

### 使用

在 LVGL 初始化后添加：

```c
#include "lvgl_remote.h"

// Wi-Fi 连接后启动
lvgl_remote_init();
lvgl_remote_start(8080);  // 端口 8080

// 在 PC 上下载客户端查看器
// https://github.com/CubeCoders/LVGLRemoteServer/releases
```

PC 端打开客户端，输入 ESP32 的 IP 地址，即可实时查看和控制设备屏幕。

---

## 方案三：串口帧缓冲传输（无 Wi-Fi 环境）

当设备没有 Wi-Fi 连接时，可以通过串口把帧缓冲数据发送到 PC。

### ESP32 端代码

```c
void screenshot_via_uart(void) {
    const uint16_t *fb = lcd_get_frame_buffer();
    uint16_t width = 172, height = 640;
    
    // 发送标记头
    const char header[] = "FBSTART\n";
    uart_write_bytes(UART_NUM_0, header, strlen(header));
    
    // 发送宽高
    uart_write_bytes(UART_NUM_0, (const char*)&width, 2);
    uart_write_bytes(UART_NUM_0, (const char*)&height, 2);
    
    // 分块发送帧缓冲数据（避免串口缓冲区溢出）
    const int chunk_size = 1024;
    for (int i = 0; i < width * height; i += chunk_size) {
        int send_size = (i + chunk_size > width * height) 
                       ? (width * height - i) * 2 
                       : chunk_size * 2;
        uart_write_bytes(UART_NUM_0, (const char*)(fb + i), send_size);
        vTaskDelay(pdMS_TO_TICKS(10));  // 避免串口拥塞
    }
    
    // 发送结束标记
    const char footer[] = "\nFBEND\n";
    uart_write_bytes(UART_NUM_0, footer, strlen(footer));
    
    ESP_LOGI("SCREENSHOT", "Frame buffer sent via UART");
}
```

### PC 端 Python 接收脚本

```python
#!/usr/bin/env python3
"""ESP32 串口截图接收工具"""

import serial
import struct
import numpy as np
from PIL import Image
import sys

PORT = '/dev/tty.usbmodem1101'  # 修改为你的串口
BAUD = 115200

ser = serial.Serial(PORT, BAUD, timeout=30)
print(f"Connected to {PORT}, waiting for screenshot...")

buffer = b''
while True:
    data = ser.read(1024)
    buffer += data
    
    # 查找帧头
    start_idx = buffer.find(b'FBSTART\n')
    if start_idx == -1:
        continue
    
    # 查找帧尾
    end_idx = buffer.find(b'\nFBEND\n', start_idx)
    if end_idx == -1:
        continue
    
    # 提取帧数据
    frame_data = buffer[start_idx + 8:end_idx]  # 跳过 FBSTART\n
    # 解析宽高（前 4 字节）
    width, height = struct.unpack('<HH', frame_data[:4])
    pixels = frame_data[4:]
    
    # RGB565 → RGB888
    img_array = np.frombuffer(pixels, dtype=np.uint16)
    r = ((img_array >> 11) & 0x1F) << 3
    g = ((img_array >> 5) & 0x3F) << 2
    b = (img_array & 0x1F) << 3
    rgb = np.stack([r, g, b], axis=-1).astype(np.uint8)
    rgb = rgb.reshape((height, width, 3))
    
    # 保存图像
    img = Image.fromarray(rgb)
    img.save('screenshot.png')
    print(f"Screenshot saved: {width}x{height}")
    
    # 清空缓冲区
    buffer = buffer[end_idx + 7:]
```

### 使用方式

```bash
# 1. 在 ESP32 串口监控中触发截图
# 发送命令: screenshot
# 或代码中调用 screenshot_via_uart()

# 2. 在另一个终端运行 Python 脚本
python3 uart_screenshot.py

# 3. 查看截图
open screenshot.png
```

---

## 推荐方案选择

| 场景 | 推荐方案 | 理由 |
|------|---------|------|
| **日常开发调试** | HTTP 截图端点 | 浏览器即可查看，最方便 |
| **LVGL UI 开发** | LVGLRemoteServer | 实时查看+远程触摸控制 |
| **无 Wi-Fi 环境** | 串口传输 | 不依赖网络，可靠 |
| **录制演示视频** | LVGLRemoteServer + OBS | 实时录屏，画质好 |
| **自动化测试** | HTTP 端点 + curl | 可集成到 CI/CD |

---

## 集成到日历项目的建议

在 `wifi_sync` 组件中，Wi-Fi 连接成功后自动启动截图服务：

```c
// wifi_sync.c
#include "screenshot_server.h"

static void on_wifi_connected(void) {
    // ... 其他同步逻辑 ...
    
    // 启动截图服务
    screenshot_server_register_fb_callback(lcd_get_frame_buffer);
    screenshot_server_start();
    
    ESP_LOGI("WIFI", "Screenshot: http://%s:8080", get_ip_address());
}
```

这样每次设备连上 Wi-Fi，日志就会输出截图地址，浏览器打开即可查看当前屏幕状态。
