# Kimi 视觉调试工作流 — 设备截图 + AI 自动分析

> Kimi 是多模态 AI，能看懂图片。设计一套工作流，让 Kimi 在开发过程中自动获取设备屏幕截图、分析 UI 问题、修复代码。

---

## 工作流概览

```
用户："屏幕显示有问题，请截图分析并修复"
  │
  ▼
Kimi 自动执行：
  1. curl 获取设备截图 (BMP)
  2. sips 转换为 PNG (Mac 自带)
  3. 读取 PNG 分析屏幕内容
  4. 发现问题
  5. 修改代码修复
```

---

## 第一步：设备端截图组件（极简版）

**仅需 2 个文件**，性能最优（零转换，< 50ms）：

### components/screenshot_server/CMakeLists.txt

```cmake
idf_component_register(
    SRCS "screenshot_server.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server
)
```

### components/screenshot_server/include/screenshot_server.h

```c
#pragma once
#include <stdint.h>
#include "esp_err.h"

void screenshot_server_register_fb(uint16_t *fb, uint16_t width, uint16_t height);
esp_err_t screenshot_server_start(uint16_t port);
```

### components/screenshot_server/screenshot_server.c

```c
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

static esp_err_t screen_handler(httpd_req_t *req) {
    if (!s_fb) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No FB");
    uint32_t pix = s_w * s_h * 2, file = 70 + pix;
    uint8_t *rsp = heap_caps_malloc(file, MALLOC_CAP_SPIRAM);
    if (!rsp) rsp = heap_caps_malloc(file, MALLOC_CAP_DEFAULT);
    if (!rsp) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
    memcpy(rsp, BMP_HDR, 70);
    *(uint32_t*)(rsp+2) = file; *(uint32_t*)(rsp+18) = s_w; *(uint32_t*)(rsp+22) = s_h; *(uint32_t*)(rsp+34) = pix;
    uint16_t *dst = (uint16_t*)(rsp+70);
    for (int y = 0; y < s_h; y++) memcpy(&dst[(s_h-1-y)*s_w], &s_fb[y*s_w], s_w*2);
    httpd_resp_set_type(req, "image/bmp");
    esp_err_t ret = httpd_resp_send(req, (const char*)rsp, file);
    free(rsp); return ret;
}
static const httpd_uri_t uri = {"/debug/screen.bmp", HTTP_GET, screen_handler, NULL};

void screenshot_server_register_fb(uint16_t *fb, uint16_t w, uint16_t h) { s_fb=fb; s_w=w; s_h=h; }
esp_err_t screenshot_server_start(uint16_t port) {
    httpd_config_t c = HTTPD_DEFAULT_CONFIG(); c.server_port=port; c.stack_size=4096;
    esp_err_t r = httpd_start(&s_server, &c); if(r)return r;
    httpd_register_uri_handler(s_server, &uri);
    ESP_LOGI(TAG, "http://<ip>:%d/debug/screen.bmp", port); return ESP_OK;
}
```

---

## 第二步：Mac 本地截图获取函数

添加到 `~/.zshrc`，终端里随时可用：

```bash
# ESP32 设备截图获取（BMP → PNG）
esp_screen() {
    local ip="${1:-192.168.1.100}"  # 默认设备IP，可覆盖
    local out="${2:-/tmp/esp_screen.png}"
    echo "📸 Fetching screenshot from $ip ..."
    curl -s "http://${ip}:8080/debug/screen.bmp" -o /tmp/esp_screen.bmp
    if [ ! -f /tmp/esp_screen.bmp ]; then
        echo "❌ Failed to download screenshot"
        return 1
    fi
    # Mac 自带 sips 转换 BMP → PNG
    sips -s format png /tmp/esp_screen.bmp --out "$out" >/dev/null 2>&1
    # 或者用 Pillow（如果安装了）
    # python3 -c "from PIL import Image; Image.open('/tmp/esp_screen.bmp').save('$out')"
    echo "✅ Screenshot saved: $out"
    echo "   Size: $(du -h $out | cut -f1)"
}

# 快速查看截图
esp_view() {
    esp_screen "$1" && open /tmp/esp_screen.png
}
```

加载：
```bash
source ~/.zshrc
```

使用：
```bash
esp_screen                    # 默认IP，保存到 /tmp/esp_screen.png
esp_screen 192.168.1.50       # 指定IP
esp_screen 192.168.1.50 ~/Desktop/bug.png  # 指定保存路径
esp_view                      # 截图并用预览打开
```

---

## 第三步：Kimi 专用 Debug Prompt（关键）

**在 AGENTS.md 末尾添加以下 Debug 段落**，Kimi 会自动在需要时使用截图功能：

```markdown
---

## Debug 调试规范 — 截图分析

### 截图端点
设备运行后提供 HTTP 截图服务：
- URL: `http://<device-ip>:8080/debug/screen.bmp`
- 格式: BMP (RGB565)
- 分辨率: 172x640

### 获取截图的步骤
当需要检查屏幕显示效果时，按以下步骤执行：

1. **获取截图**（执行 shell 命令）：
   ```bash
   curl -s http://192.168.1.100:8080/debug/screen.bmp -o /tmp/esp_screen.bmp
   ```
   如果设备IP不同，使用实际的IP地址。

2. **转换为 PNG**（Mac 自带 sips）：
   ```bash
   sips -s format png /tmp/esp_screen.bmp --out /tmp/esp_screen.png
   ```
   或者用 Python：
   ```bash
   python3 -c "from PIL import Image; Image.open('/tmp/esp_screen.bmp').save('/tmp/esp_screen.png')"
   ```

3. **分析截图**：
   读取 `/tmp/esp_screen.png`，检查：
   - UI 布局是否正确（元素位置、对齐）
   - 文字是否完整显示（有无截断、溢出）
   - 颜色是否正确（与预期一致）
   - 触摸区域是否可见
   - 状态栏信息是否正确

4. **记录发现**：
   把截图中发现的问题列出，每个问题包括：
   - 问题描述
   - 在截图中的具体位置（坐标或区域）
   - 修复建议

### 何时使用截图调试
- 用户报告"屏幕显示有问题"时
- UI 修改后需要验证效果时
- 布局、对齐、文字截断问题时
- 颜色、样式不符合预期时
- 触摸交互无响应时（检查触摸区域是否绘制正确）

### 截图分析检查清单
检查以下项目：
- [ ] 状态栏显示正确（日期、时间、星期、WiFi图标）
- [ ] 课程卡片完整可见（无截断、无重叠）
- [ ] 课程名称、时间、老师、地点文字清晰
- [ ] 颜色与课程类型匹配
- [ ] 当前课程高亮显示
- [ ] 月历视图日期正确
- [ ] 无乱码、无闪烁、无花屏
- [ ] 背景色一致（无异常色块）
```

---

## 第四步：使用示例

### 场景 1：用户主动要求截图分析

**用户输入：**
> 课程卡片的文字被截断了，请截图看看并修复

**Kimi 自动执行：**
```bash
# 1. 获取截图
curl -s http://192.168.1.100:8080/debug/screen.bmp -o /tmp/esp_screen.bmp
sips -s format png /tmp/esp_screen.bmp --out /tmp/esp_screen.png

# 2. 分析截图
# "我看到第3个课程卡片的'老师'字段显示不完整，
#  '王老师'只显示了'王'，后面的被截断了。
#  原因：卡片宽度不够，文字区域只有 120px，
#  中文字体每个字约 16px，'王老师'需要 32px+padding。
#  修复方案：增加卡片宽度或缩小字体..."

# 3. 修改代码修复
# 编辑 components/ui_calendar/ui_calendar.c
# 调整 lv_obj_set_width(label, ...) 参数
```

### 场景 2：Kimi 主动截图验证修改

**Kimi 完成 UI 代码修改后：**
> 代码已修改，让我截图验证效果...
> （自动执行 esp_screen 获取截图）
> （分析截图）"布局已修复，文字完整显示，颜色正确。"

### 场景 3：终端手动使用

```bash
# 快速截图查看
esp_view

# 或者在 kimi cli 中
$ esp_screen
✅ Screenshot saved: /tmp/esp_screen.png
   Size: 120K

# 然后把 /tmp/esp_screen.png 给 Kimi 分析
```

---

## 性能数据

| 指标 | 数值 |
|------|------|
| 截图 CPU 时间 | < 50ms（纯 memcpy） |
| Wi-Fi 传输时间（局域网） | ~200-500ms（220KB BMP） |
| BMP → PNG 转换 | ~100ms（sips） |
| **总时间** | **~0.5-1 秒/张** |
| 内存占用 | 440KB（PSRAM，不影响主程序） |

---

## 常见问题

**Q: Kimi CLI 能直接读取 PNG 文件分析吗？**
A: Kimi 是多模态 AI，支持图片输入。把 `/tmp/esp_screen.png` 文件路径告诉它，它可以读取并分析。如果 CLI 版本不支持文件图片输入，可以把图片转为 base64 或描述问题。

**Q: sips 命令不存在？**
A: sips 是 Mac 系统自带工具。如果不存在，用 Python：
```bash
pip3 install Pillow
python3 -c "from PIL import Image; Image.open('/tmp/esp_screen.bmp').save('/tmp/esp_screen.png')"
```

**Q: 截图端点返回 404？**
A: 确保 Wi-Fi 已连接且截图服务已启动。检查日志中的设备 IP：
```
I SCREENSHOT: http://<ip>:8080/debug/screen.bmp
```

**Q: 截图是花的/乱码？**
A: 帧缓冲指针可能不正确。检查 `screenshot_server_register_fb()` 传入的指针是否与 LCD 驱动实际使用的帧缓冲区一致。
