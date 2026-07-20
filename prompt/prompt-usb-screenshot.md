# Prompt：USB-Serial 截图方案（直接复制发给 Coding Agent）

> 通过现有 USB 连接（USB-Serial-JTAG）传输帧缓冲数据到 PC，无需 Wi-Fi，不依赖网络。

---

## 复制这段发给 Kimi CLI

```
请为项目添加 USB-Serial 截图功能。通过现有的 USB 数据线连接，把帧缓冲数据发送到 Mac，不需要 Wi-Fi。

## 技术方案

通过 UART（USB-Serial-JTAG）发送二进制帧缓冲数据，Mac 用 Python 脚本接收并转换为 PNG。

**波特率提升到 2Mbps 加速传输。**

## 帧格式（二进制协议）

```
[FB_START\n]   -- 8字节 ASCII 帧头
[width: uint16_LE]   -- 2字节小端
[height: uint16_LE]  -- 2字节小端  
[size: uint32_LE]    -- 4字节小端 (width*height*2)
[RGB565 pixel data...] -- width*height*2 字节
[FB_END\n]     -- 6字节 ASCII 帧尾
```

## 需要创建的文件

### 1. components/usb_screenshot/usb_screenshot.h

```c
#pragma once
#include <stdint.h>
#include "esp_err.h"

// 注册帧缓冲区
void usb_screenshot_register_fb(uint16_t *fb, uint16_t w, uint16_t h);

// 发送截图（通过 UART）
void usb_screenshot_send(void);

// 启动自动截图任务（可选，每 N 秒自动发送）
void usb_screenshot_start_auto(uint32_t interval_ms);
```

### 2. components/usb_screenshot/usb_screenshot.c

```c
#include "usb_screenshot.h"
#include <string.h>
#include "esp_log.h"
#include "driver/uart.h"
#include "esp_heap_caps.h"

static const char *TAG = "USB_SS";
static uint16_t *s_fb = NULL;
static uint16_t s_w = 172, s_h = 640;

void usb_screenshot_register_fb(uint16_t *fb, uint16_t w, uint16_t h) {
    s_fb = fb; s_w = w; s_h = h;
    ESP_LOGI(TAG, "FB: %dx%d", w, h);
}

void usb_screenshot_send(void) {
    if (!s_fb) { ESP_LOGE(TAG, "No FB"); return; }
    
    uint32_t pix_size = s_w * s_h * 2;
    uint32_t total = 8 + 8 + pix_size + 6; // head + meta + data + tail
    
    // 在 PSRAM 中组装完整帧
    uint8_t *buf = heap_caps_malloc(total, MALLOC_CAP_SPIRAM);
    if (!buf) buf = heap_caps_malloc(total, MALLOC_CAP_DEFAULT);
    if (!buf) { ESP_LOGE(TAG, "OOM"); return; }
    
    uint8_t *p = buf;
    
    // 帧头
    memcpy(p, "FB_START\n", 8); p += 8;
    
    // 元数据（小端序）
    *(uint16_t*)p = s_w;       p += 2;
    *(uint16_t*)p = s_h;       p += 2;
    *(uint32_t*)p = pix_size;  p += 4;
    
    // 像素数据（直接拷贝，不翻转Y轴，PC端处理）
    memcpy(p, s_fb, pix_size); p += pix_size;
    
    // 帧尾
    memcpy(p, "FB_END\n", 6);
    
    // 通过 UART0 发送（一次性发送，利用 FIFO）
    uart_write_bytes(UART_NUM_0, (const char*)buf, total);
    
    free(buf);
    ESP_LOGI(TAG, "Sent: %lu bytes", (unsigned long)total);
}
```

### 3. components/usb_screenshot/CMakeLists.txt

```cmake
idf_component_register(
    SRCS "usb_screenshot.c"
    INCLUDE_DIRS "."
    REQUIRES driver
)
```

### 4. PC端接收脚本：~/esp32-tools/receive_screen.py

```python
#!/usr/bin/env python3
"""ESP32 USB-Serial 截图接收工具"""

import serial
import struct
import numpy as np
from PIL import Image
import sys
import os

DEFAULT_PORT = "/dev/tty.usbmodem1101"
BAUDRATE = 2000000  # 2Mbps，与设备端匹配
OUTPUT = "/tmp/esp_screen.png"

def rgb565_to_rgb888(pixels):
    """RGB565 → RGB888 numpy 数组"""
    arr = np.frombuffer(pixels, dtype=np.uint16)
    r = ((arr >> 11) & 0x1F) << 3
    g = ((arr >> 5) & 0x3F) << 2
    b = (arr & 0x1F) << 3
    return np.stack([r, g, b], axis=-1).astype(np.uint8)

def receive_screenshot(ser):
    """从串口接收一帧截图"""
    buf = b""
    
    # 1. 等待帧头
    while b"FB_START\n" not in buf:
        chunk = ser.read(1024)
        if chunk:
            buf += chunk
    
    start_idx = buf.find(b"FB_START\n")
    buf = buf[start_idx + 8:]  # 去掉帧头
    
    # 2. 读取元数据（8字节）
    while len(buf) < 8:
        buf += ser.read(8 - len(buf))
    
    width, height, pix_size = struct.unpack('<HH I', buf[:8])
    buf = buf[8:]
    
    print(f"Receiving: {width}x{height}, {pix_size} bytes...")
    
    # 3. 读取像素数据
    while len(buf) < pix_size + 6:
        needed = (pix_size + 6) - len(buf)
        buf += ser.read(min(needed, 4096))
    
    # 4. 检查帧尾
    if buf[pix_size:pix_size+6] != b"FB_END\n":
        print("Warning: Frame tail mismatch")
    
    pixels = buf[:pix_size]
    return width, height, pixels

def save_png(width, height, pixels, path):
    """保存为 PNG"""
    rgb = rgb565_to_rgb888(pixels)
    # BMP 需要 Y 轴翻转
    rgb = np.flipud(rgb.reshape(height, width, 3))
    img = Image.fromarray(rgb)
    img.save(path)
    print(f"Saved: {path} ({width}x{height})")

def main():
    port = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_PORT
    out = sys.argv[2] if len(sys.argv) > 2 else OUTPUT
    
    print(f"Connecting to {port} @ {BAUDRATE} baud...")
    
    try:
        ser = serial.Serial(port, BAUDRATE, timeout=30)
    except Exception as e:
        print(f"Error: {e}")
        print(f"Available ports:")
        os.system("ls /dev/tty.*")
        return 1
    
    print("Waiting for screenshot... (trigger from device)")
    
    try:
        w, h, pix = receive_screenshot(ser)
        save_png(w, h, pix, out)
        print(f"Done. Open: open {out}")
    except KeyboardInterrupt:
        print("\nInterrupted")
    finally:
        ser.close()
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
```

### 5. PC端快速查看脚本：~/esp32-tools/view_screen.sh

```bash
#!/bin/bash
# 一键接收并打开截图

PORT="${1:-/dev/tty.usbmodem1101}"
OUT="/tmp/esp_screen.png"

echo "Receiving screenshot from $PORT..."
python3 ~/esp32-tools/receive_screen.py "$PORT" "$OUT"

if [ -f "$OUT" ]; then
    echo "Opening $OUT..."
    open "$OUT"
fi
```

```bash
chmod +x ~/esp32-tools/view_screen.sh
```

---

## 波特率配置

### sdkconfig.defaults 中添加：

```
CONFIG_ESPTOOLPY_FLASHFREQ_80M=y
CONFIG_ESPTOOLPY_FLASHMODE_QIO=y
CONFIG_ESPTOOLPY_BAUD=2000000
```

或者在 menuconfig 中：
- Serial flasher config → `idf.py monitor` baud rate → 2000000

### 设备端 UART 初始化（在 main.c 中）：

```c
#include "driver/uart.h"

// 初始化 UART0 为 2Mbps
uart_config_t uart_cfg = {
    .baud_rate = 2000000,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
};
uart_param_config(UART_NUM_0, &uart_cfg);
uart_driver_install(UART_NUM_0, 4096, 4096, 0, NULL, 0);
```

**注意**：ESP32-S3 的 USB-Serial-JTAG 默认波特率是 460800，但支持到 **3Mbps**。2Mbps 是稳定工作的上限。

---

## 使用方式

### 方式一：命令行触发截图（手动）

**设备端**（在串口 monitor 中发送命令）：
```
> screenshot
```

代码中需要在串口输入处理中添加：
```c
if (strcmp(input, "screenshot") == 0) {
    usb_screenshot_send();
}
```

**PC端**（另一个终端窗口）：
```bash
~/esp32-tools/view_screen.sh
# 或
python3 ~/esp32-tools/receive_screen.py /dev/tty.usbmodem1101
```

### 方式二：代码中自动触发（调试时）

在 UI 初始化完成后自动发送一张截图：
```c
// 在 ui_calendar 初始化后
usb_screenshot_send();
ESP_LOGI("UI", "Screenshot sent for verification");
```

### 方式三：Kimi 自动截图分析

在 AGENTS.md 中添加：
```markdown
## USB 截图调试

当需要检查屏幕显示时，通过 USB-Serial 获取截图：

1. 在串口 monitor 中发送命令：`screenshot`
2. 在另一个终端执行：`~/esp32-tools/view_screen.sh`
3. 分析 /tmp/esp_screen.png

波特率：2Mbps
传输时间：~0.9 秒（220KB）
```

---

## 在 LCD 驱动中注册帧缓冲

在 axs15231b_display.c 初始化后：
```c
#include "usb_screenshot.h"

// 帧缓冲分配后注册
usb_screenshot_register_fb(frame_buffer, 172, 640);
```

---

## 现在请执行

1. 创建 usb_screenshot 组件（3 个文件）
2. 创建 PC 端 Python 脚本（~/esp32-tools/ 目录）
3. 修改 LCD 驱动注册帧缓冲
4. 在 main.c 中初始化 UART 为 2Mbps
5. 添加串口命令处理（输入 "screenshot" 触发）
6. 编译验证

完成后告诉我如何在 monitor 中触发截图和 PC 端如何接收。
```

---

## 使用步骤总结

```bash
# 终端 1：连接设备串口（monitor 模式）
idf.py -p /dev/tty.usbmodem1101 -b 2000000 monitor
# 在 monitor 中输入：screenshot

# 终端 2：接收截图（另一个窗口）
~/esp32-tools/view_screen.sh
# 或
python3 ~/esp32-tools/receive_screen.py

# 截图自动保存到 /tmp/esp_screen.png 并打开预览
```

## 性能

| 指标 | 数值 |
|------|------|
| 波特率 | 2Mbps |
| 数据量 | 220KB + 22字节头尾 |
| 传输时间 | **~0.9 秒** |
| 是否需要 Wi-Fi | **不需要** |
| 是否需要额外硬件 | **不需要**（现有 USB 线） |
