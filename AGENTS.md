# Agent Guide — ESP32-S3-Touch-LCD-3.49 Repository

> This file is written for AI coding agents. It describes the whole repository: what it is, how it is laid out, how to build each part, and the conventions and pitfalls to respect. English is the primary language of source code and most documentation; some inline comments and the notes under `prompt/` are in Chinese.

---

## 1. Project Overview

This repository centers on the **Waveshare ESP32-S3-Touch-LCD-3.49** development board (ESP32-S3, 16 MB flash, octal PSRAM, 172×640 AXS15231B QSPI display with built-in touch). It contains three distinct kinds of content:

1. **Vendor example firmware** (`Examples/Arduino`, `Examples/ESP-IDF`) — the original Waveshare demo projects, one per peripheral, plus prebuilt binaries in `Firmware/`.
2. **`kids-calendar/`** — the **active development project**: an ESP-IDF application that turns the board into a kids' calendar/course-schedule display using LVGL 9. This is where new work happens.
3. **Support material** — `Arduino_Libraries/` (libraries for the Arduino examples), `prompt/` (Chinese-language working notes/prompts used to drive AI-assisted development, e.g. screenshot-debug workflows), and `.codegraph/` (a local code index; ignore it).

Product wikis: https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-3.49 (English), https://www.waveshare.net/wiki/ESP32-S3-Touch-LCD-3.49 (中文).

> Note: `kid-calendar/` (singular) is an **empty leftover directory**; the real app is `kids-calendar/` (plural). Do not put files in `kid-calendar/`.

There are **no** `pyproject.toml`, `package.json`, or `Cargo.toml` files. All dependency management is done through the **ESP-IDF Component Manager** (`idf_component.yml` → `dependencies.lock` → `managed_components/`) or, for Arduino, by manually copying libraries from `Arduino_Libraries/`.

---

## 2. Repository Layout

```text
├── README.md                 # Links to Waveshare wikis + Arduino IDE config screenshot
├── AGENTS.md                 # This file
├── Examples/
│   ├── Arduino/              # 10 Arduino examples (01_ADC_Test … 10_LVGL_V9_Test)
│   └── ESP-IDF/              # 11 ESP-IDF examples (01_ADC_Test … 11_FactoryProgram)
│       └── AGENTS.md         # Detailed guide for the ESP-IDF examples — read it before
│                             #   touching anything under Examples/ESP-IDF
├── Arduino_Libraries/        # lvgl8/, lvgl9/, SensorLib/ to copy into the Arduino IDE
│                             #   libraries folder (lvgl8 and lvgl9 are mutually exclusive)
├── Firmware/                 # Prebuilt factory + xiaozhi (voice assistant) .bin images
├── kids-calendar/            # ACTIVE PROJECT: kids calendar app (ESP-IDF, LVGL 9, C)
│   ├── CMakeLists.txt        # Top-level project file (project(kids-calendar))
│   ├── partitions.csv        # nvs / phy_init / factory (8 MB app, no OTA partition)
│   ├── sdkconfig.defaults    # Checked-in minimal Kconfig (target esp32s3, 16 MB flash,
│   │                         #   octal PSRAM, FreeRTOS 1000 Hz, LVGL fonts incl. CJK)
│   ├── sdkconfig             # Generated full config — do not edit by hand
│   ├── dependencies.lock     # Locked component versions
│   ├── main/
│   │   ├── main.c            # app_main: QSPI LCD init, LVGL 9 port, touch read, RTC, UI
│   │   ├── user_config.h     # Single source of truth for pins, resolution, rotation
│   │   ├── Kconfig.projbuild # "Calendar App Configuration" menu (Wi-Fi, API, NTP, TZ)
│   │   ├── idf_component.yml # lvgl/lvgl ^9, espressif/esp_lcd_axs15231b ^1.0.1
│   │   └── CMakeLists.txt    # main component: PRIV_REQUIRES ui_calendar, i2c_bsp,
│   │                         #   lcd_bl_pwm_bsp, pcf85063_rtc, usb_screenshot, ...
│   ├── components/           # Local components (see §4)
│   ├── managed_components/   # Fetched by Component Manager — never edit
│   └── build/                # Build output (currently built with ESP-IDF 5.5.3)
├── prompt/                   # Chinese working notes: screenshot-debug plans and prompts
│                             #   used with AI coding agents (historical/reference)
└── .codegraph/               # Local code-index database — tooling artifact, ignore
```

---

## 3. Technology Stack

### kids-calendar (active project)

| Layer | Technology |
|---|---|
| MCU | ESP32-S3, 16 MB flash, octal PSRAM @ 80 MHz |
| SDK | ESP-IDF — originally configured for 5.4.2 (`sdkconfig.defaults` header), currently built with **5.5.3** (`CONFIG_IDF_INIT_VERSION="5.5.3"`) |
| Language | C |
| RTOS | FreeRTOS, `CONFIG_FREERTOS_HZ=1000` |
| Graphics | LVGL 9.x via Component Manager (`lvgl/lvgl ^9`), full-render mode, double frame buffers in PSRAM + small DMA buffer |
| Display | AXS15231B over QSPI (SPI3_HOST, 40 MHz), driver `espressif/esp_lcd_axs15231b`; native 172×640, RGB565 |
| Touch | AXS15231B built-in touch over raw I2C at address `0x3B` (GPIO17/18), polled 11-byte command sequence |
| RTC | PCF85063 over I2C (GPIO47/48), `pcf85063_rtc` component |
| IMU | QMI8658 accelerometer over I2C (same bus as RTC, addr 0x6B), `qmi8658_imu`; auto-rotates the UI across all **four** orientations (0/90/180/270, gravity on Y == landscape per on-hardware calibration) |
| Wi-Fi | STA via `wifi_sync` (Kconfig credentials, ESP32-S3 is **2.4 GHz only** — a 5 GHz SSID will never be found); SNTP time sync → RTC, Beijing timezone |
| Battery | Voltage via ADC1_CH3 (GPIO4), 3:1 divider, `battery_bsp`; percent shown in status bar |
| Backlight | PWM on GPIO42 via `lcd_bl_pwm_bsp` |
| Fonts | Montserrat 20 (clock) + custom `lv_font_chinese_14/16` (NotoSansSC, ASCII + UI-specific CJK glyphs, plain bitmap) for all other text; the built-in Source Han Sans CJK fonts are still enabled in Kconfig but unused by the UI (they lack several needed glyphs) |

### Examples

See `Examples/ESP-IDF/AGENTS.md` for the full table. In short: 11 self-contained ESP-IDF projects (C and C++), LVGL 8 for `07`–`09`/`11`, LVGL 9 for `10`; plus 10 Arduino IDE examples using the libraries in `Arduino_Libraries/`.

---

## 4. kids-calendar Code Organization

### `main/main.c` — entry point and LVGL porting layer

`app_main()` does, in order: touch I2C init → QSPI bus + AXS15231B panel install (300 ms pre-init delay + GPIO21 reset pulse) → LVGL init (display, flush callback with RGB565 byte swap, double PSRAM buffers, `LV_DISPLAY_RENDER_MODE_FULL`) → touch input device → LVGL tick timer (5 ms) → LVGL handler task (mutex-protected `lv_timer_handler()`, pinned to core 0) → UI setup (`ui_init`, demo course timeline, month calendar) → USB screenshot registration (+LVGL-lock hooks) and command task → RTC init and a **temporary hard-coded default time** (NTP sync planned, not implemented) → battery ADC init → IMU init + orientation task (rotates UI on device flip) → main loop that refreshes the status bar and battery once per minute.

The flush callback copies rotated/full frames through a small DMA buffer (`trans_buf_1`) in `LVGL_SPIRAM_BUFF_LEN / LVGL_DMA_BUFF_LEN` chunks, synchronized with `flush_done_semaphore`. Touch coordinates are mapped differently depending on `Rotated` in `user_config.h` (currently `USER_DISP_ROT_NONO`).

### `main/user_config.h` — board configuration

The single source of truth for: QSPI LCD pins, display resolution (172×640), DMA/PSRAM buffer lengths, software rotation flag, backlight test flag, touch I2C pins/address, RTC I2C pins. It targets the **V2 (PCB Rev2.0) pinout**; LCD_RST and TP_INT go through a TCA9554 I/O expander and are intentionally left uncontrolled (`-1`) here.

### `components/`

| Component | State | Responsibility |
|---|---|---|
| `i2c_bsp` | Used | Shared I2C bus init (touch bus, `disp_touch_dev_handle`) |
| `lcd_bl_pwm_bsp` | Present, **not called** | Backlight PWM — disabled: on this board the backlight needs no driving (see pitfall §10.8); calling it with the wrong pin kept the screen black |
| `pcf85063_rtc` | Used | PCF85063 driver: `rtc_init`, `rtc_set_time`, `rtc_get_time`; owns the RTC I2C bus (I2C_NUM_0) and exports it via `pcf85063_get_bus()` for IMU/other devices |
| `battery_bsp` | Used | Battery voltage/percent: ADC1_CH3 (GPIO4), 3:1 divider, curve-fitting calibration. **No TCA9554 access** — writing it kills the panel on this board |
| `qmi8658_imu` | Used | Pure-C QMI8658 accelerometer (addr 0x6B, on RTC I2C bus); `imu_init`, `imu_read_accel` |
| `ui_calendar` | Used | All screens: status bar (incl. battery icon/percent), course timeline, month calendar, reminders, styles, CJK fonts, `ui_set_orientation()` for portrait/landscape rebuild, plus `ui_screenshot.c` |
| `usb_screenshot` | Used | Dumps the RGB565 frame buffer over USB-Serial-JTAG (native USB, `/dev/tty.usbmodem*`) for AI-assisted visual debugging; **on-demand** — sends one frame when it receives the `screenshot` command; frame grab runs under the LVGL mutex + `lv_refr_now()` (no tearing) |
| `screenshot_server` | Written, **not wired in** | HTTP server serving the frame buffer as BMP (`/screen`) for Wi-Fi-based screenshot debugging |
| `wifi_sync` | Used | Wi-Fi STA + NTP time sync: connects with Kconfig credentials, SNTP → writes RTC (Beijing TZ, re-sync every 6 h), drives the status-bar WiFi indicator. Needs `nvs_flash_init()` before `esp_wifi_init` |

The `course_t` data model (`id[32]`, `name[64]`, `day_of_week`, `start_time[8]`/`end_time[8]`, `teacher[32]`, `location[64]`, `color[8]`, `remind_before`) is defined in `ui_calendar/include/ui_calendar.h`; `main.c` currently feeds it hard-coded demo courses (`s_demo_courses[]`) with Chinese names — only characters covered by `lv_font_chinese_14/16` may be used. The fonts are generated with:

```bash
lv_font_conv --font <NotoSansSC ttf> --size <14|16> --bpp 4 --format lvgl \
  --no-compress --no-prefilter --range 0x20-0x7E --symbols "<CJK chars>" \
  --output components/ui_calendar/lv_font_chinese_<size>.c
```

(`--no-compress --no-prefilter` is required — LVGL's fmt_txt renderer draws nothing for `bitmap_format=1` data from lv_font_conv 1.5.3. Also fix the generated `#include "lvgl/lvgl.h"` to `#include "lvgl.h"`.)

---

## 5. Build, Flash, and Configuration

Prerequisite: ESP-IDF installed and environment sourced (`idf.py` on PATH, `$IDF_PATH` set). Target is always `esp32s3`.

### kids-calendar

```bash
cd kids-calendar
idf.py set-target esp32s3   # only needed on a fresh checkout
idf.py build
idf.py flash monitor        # or: idf.py -p <PORT> flash monitor
```

- Kconfig changes: `idf.py menuconfig` (app options live under **Calendar App Configuration**); persist defaults with `idf.py save-defconfig`.
- Dependencies re-resolve automatically on build; force with `idf.py reconfigure` or by deleting `managed_components/` + `dependencies.lock`.
- Build artifacts: `build/kids-calendar.bin`, `build/bootloader/bootloader.bin`, `build/partition_table/partition-table.bin`. Flash layout: bootloader `0x0`, partition table `0x8000`, app `0x10000`.

### Examples (ESP-IDF / Arduino)

Each example under `Examples/ESP-IDF/` builds independently with the same `idf.py set-target esp32s3 && idf.py build flash monitor` flow — details, quirks (e.g. mismatched `project()` names), and the V1/V2 pinout split are documented in `Examples/ESP-IDF/AGENTS.md`. Arduino examples are built in the Arduino IDE after copying `Arduino_Libraries/` contents into the IDE's libraries folder (only one of `lvgl8`/`lvgl9` at a time); required IDE settings are shown in `Tools Configuration.png`.

---

## 6. Development Conventions

- **Minimal, localized changes.** Match the surrounding style — indentation is inconsistent (2-space, 4-space, tabs all appear, sometimes in one file).
- BSP-style component naming: `<peripheral>_bsp.c/.h`; headers use `#pragma once` or include guards; C APIs that may be called from C++ are wrapped in `extern "C"`.
- Error handling uses `ESP_ERROR_CHECK(...)` / `ESP_ERROR_CHECK_WITHOUT_ABORT(...)` and `assert(...)` for allocations.
- LVGL is not thread-safe: **every LVGL API call outside the LVGL task must hold `lvgl_mux`** (see the `example_lvgl_lock(100)` pattern in the main loop).
- Pin/config changes belong in `user_config.h`, not scattered through source.
- Never edit `managed_components/`, `sdkconfig` (by hand), or `build/` output.
- The repo root is a git checkout of the Waveshare examples; many files under `Examples/` show as locally modified (V2 pinout adaptation). `kids-calendar/` and `prompt/` are untracked local work.

---

## 7. Testing and Validation

There are **no automated tests or CI** anywhere in this repository. Validation is manual, on hardware:

1. `idf.py build` — must compile cleanly.
2. `idf.py flash monitor` — watch serial logs.
3. Interact with the board (touch, screen content, RTC time in the status bar).

A distinctive workflow exists for UI verification: the firmware can **dump its frame buffer to the PC as a screenshot**, so an AI agent can fetch the image (HTTP BMP via `screenshot_server`, or raw RGB565 over USB serial via `usb_screenshot`), inspect it visually, and fix UI bugs. The design notes for this live in `prompt/` (Chinese). The USB path is **on-demand**: run `python3 ~/esp32-tools/receive_screen.py` (pure stdlib, no pip deps) — it sends the `screenshot` command over USB-Serial-JTAG, receives one frame, and saves `/tmp/esp_screen.png`; typing `screenshot` in `idf.py monitor` works too (use `--listen` on the PC side is *not* combinable with an open monitor — only one process can hold the port).

---

## 8. Deployment

Manual only: `idf.py flash`. There is no OTA (the partition table has a single 8 MB `factory` app partition), no release packaging, and no flash scripts beyond what ESP-IDF generates. Prebuilt vendor images (factory test, xiaozhi voice assistant) are in `Firmware/` and can be flashed directly with `esptool.py` if stock firmware is needed.

---

## 9. Security Considerations

- No secure boot or flash encryption is configured.
- Wi-Fi credentials are Kconfig defaults (`YOUR_WIFI_SSID` / `YOUR_WIFI_PASSWORD`) — **do not commit real passwords**; `screenshot_server` would expose the screen over HTTP with no authentication, so only enable it on trusted networks.
- Demo/evaluation-grade code: `ESP_ERROR_CHECK` aborts on failure, and `main.c` sets a hard-coded RTC time at boot.
- The USB screenshot path sends raw frame data on demand over USB-Serial-JTAG; treat it as debug-only.

---

## 10. Common Pitfalls

1. **`kid-calendar/` vs `kids-calendar/`** — the singular-named directory is empty; work in the plural one.
2. **V1 vs V2 pinout** — `kids-calendar` and `10_LVGL_V9_Test` use the V2 pinout; most other ESP-IDF examples use V1. Never copy `user_config.h` across without checking.
3. **LVGL 8 vs LVGL 9** — APIs are incompatible; `kids-calendar` is LVGL 9.
4. **CJK text** — Chinese strings only render if the glyphs exist in `lv_font_chinese_14/16` (ASCII 0x20–0x7E + a fixed CJK symbol set incl. `·` U+00B7); missing glyphs show as boxes or nothing. When adding UI text, check coverage and regenerate the fonts if needed (command in §4). Also verify non-ASCII, non-CJK chars (like `·`) — they are easy to miss.
5. **Screenshot channel is USB-Serial-JTAG, not UART0** — the frame dump goes over the native USB CDC port (`/dev/cu.usbmodem*`), on-demand only (send the `screenshot` command to trigger one frame). Logs share the same channel, so the PC receiver resyncs on `FB_START`. Don't send screenshots over `UART_NUM_0` — that port isn't wired to the Mac. The frame buffer is **big-endian** RGB565 (flush does `lv_draw_sw_rgb565_swap`); the receiver decodes accordingly and upscales ×3 nearest-neighbor (`--scale N`).
6. **After `idf.py flash`, power-cycle the board** — on this board's USB-Serial-JTAG port, esptool's "hard reset" leaves the chip in download mode (`waiting for download`), so the freshly flashed app does not run until the USB cable is replugged. Also avoid asserting DTR/RTS from host tools: those lines feed the ROM's reset/boot-mode emulation. Use `/dev/cu.usbmodem*`, not `/dev/tty.usbmodem*` (writes on `tty.*` can block).
7. **Do NOT write the TCA9554** — the 01_ADC_Test (V1) pattern of pulling EXIO1 low as "battery measure enable" **kills the panel on this board** (black screen, app still alive). Battery voltage works with plain ADC1_CH3 reads, no expander involvement. `battery_bsp` deliberately does no TCA9554 I/O.
8. **Backlight needs no driving on this board** — the vendor factory/demo programs never touch the backlight pin and the screen stays lit. Earlier "V2" pinout notes (BK=GPIO42 PWM, LCD_RST via TCA9554 EXIO5) were wrong here: `user_config.h` uses BK=GPIO8 (unused), LCD_RST=GPIO21 direct, plus a 300 ms pre-init delay for cold-boot panel readiness.
9. **Panel wants big-endian RGB565** — keep `lv_draw_sw_rgb565_swap()` in the flush callback; removing it makes physical colors wrong (the demo-8 example without swap only *looks* acceptable at a glance). The screenshot receiver decodes big-endian to match.
10. **Stub components** — `screenshot_server` integration is unfinished; Kconfig options for the course API exist but nothing consumes them yet.
11. **QMI8658 multi-byte reads need CTRL1 bit6 (ADDR_AI)** — without address auto-increment, reading the 6 accel bytes from 0x35 returns the same word 3 times (x == y == z garbage). Set CTRL1=0x40 after soft reset (SensorLib does this in `reset()`).
12. **Wi-Fi needs `nvs_flash_init()` first** — calling `esp_wifi_init()` without NVS aborts with `wifi osi_nvs_open fail` (boot loop). Keep Wi-Fi failures non-fatal (graceful degrade, no `ESP_ERROR_CHECK` in the component).
