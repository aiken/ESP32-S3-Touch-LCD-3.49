# Agent Guide — ESP32-S3-Touch-LCD-3.49 / Examples / ESP-IDF

> This file is written for AI coding agents that need to work on the ESP-IDF example projects in this repository.  It describes the project layout, build system, runtime architecture, coding conventions, and practical workflows.  English is the primary language used in source code and most comments; a small number of inline comments are in Chinese.

---

## 1. Project Overview

This directory (`Examples/ESP-IDF`) contains **11 self-contained ESP-IDF example projects** for the Waveshare **ESP32-S3-Touch-LCD-3.49** development board.  Each example lives in its own folder and can be built independently.

| Folder | Purpose | Language | Key hardware exercised |
|---|---|---|---|
| `01_ADC_Test` | ADC battery/voltage reading | C | ADC via TCA9554 IO expander |
| `02_I2C_PCF85063` | PCF85063 RTC over I2C | C++ | RTC, TCA9554 |
| `03_I2C_QMI8658` | QMI8658 IMU over I2C | C++ | IMU, TCA9554 |
| `04_SD_Card` | SD card read/write over SPI | C | SD card |
| `05_WIFI_AP` | Wi-Fi SoftAP | C | Wi-Fi |
| `06_WIFI_STA` | Wi-Fi station scan / connect | C | Wi-Fi |
| `07_BATT_PWR_Test` | Power-button / power-control UI | C++ | Display, touch, LVGL v8, power GPIO |
| `08_Audio_Test` | Audio record/playback | C++ | I2S audio, codec board, display, LVGL v8 |
| `09_LVGL_V8_Test` | LVGL v8 demo (`lv_demo_widgets`) | C++ | Display, touch, backlight PWM |
| `10_LVGL_V9_Test` | LVGL v9 demo (`lv_demo_widgets`) | C | Display, touch, backlight PWM |
| `11_FactoryProgram` | Full factory test UI (all peripherals) | C++ | Display, touch, RTC, IMU, SD, Wi-Fi, BLE, audio, ADC, buttons |

All examples target the **ESP32-S3** with **16 MB flash** and **octal PSRAM**.  The display panel is an **AXS15231B** driven over QSPI, and touch input is read via a raw I2C command sequence (GT911 / CST816-style controller).

> **Important — hardware revision split:**  The examples currently use **two different pinouts**.  Examples `07`, `08`, `09`, and `11` use the older V1 pinout.  `10_LVGL_V9_Test` is currently edited to the V2 pinout (see `10_LVGL_V9_Test/main/user_config.h`).  Always check `user_config.h` before building or copying code between examples.

---

## 2. Technology Stack

| Layer | Technology / Version | Notes |
|---|---|---|
| MCU | ESP32-S3 (Xtensa LX7, 16 MB flash, octal PSRAM) | `CONFIG_IDF_TARGET="esp32s3"` |
| SDK | ESP-IDF 5.3.2 / 5.4.2 originally; active build environment may be newer (e.g. 5.5.3) | `sdkconfig.defaults` headers, `dependencies.lock` |
| RTOS | FreeRTOS | `CONFIG_FREERTOS_HZ=1000` |
| Graphics v8 | LVGL 8.x | Examples `07`, `08`, `09`, `11` |
| Graphics v9 | LVGL 9.x | Example `10` only |
| Display controller | AXS15231B over QSPI | `espressif/esp_lcd_axs15231b` |
| Touch controller | GT911 / CST816-style | Raw I2C protocol, address `0x3B` |
| RTC | PCF85063 (addr `0x51`) | Via `SensorLib` or direct I2C |
| IMU | QMI8658 (addr `0x6B`) | Via `SensorLib` |
| IO expander | TCA9554 (addr `0x20`) | `espressif/esp_io_expander_tca9554` |
| Audio | ESP codec board / `esp_codec_dev` | Example `08` and `11` |
| Sensor abstraction | Lewis He `SensorLib` | Vendored in `components/SensorLib` |
| UI design | NXP GUI Guider | Generated code under `components/ui_bsp/generated/` |

There are **no** `pyproject.toml`, `package.json`, `Cargo.toml`, or other language-specific package manifests at this level.  Dependency management is handled by the ESP-IDF Component Manager via `main/idf_component.yml` and, after resolution, `dependencies.lock`.

---

## 3. Project Structure

Each example follows the standard ESP-IDF project layout:

```text
<Example>/
├── CMakeLists.txt              # Project-level CMake file
├── sdkconfig                   # Fully resolved Kconfig output (generated)
├── sdkconfig.defaults          # Hand-edited minimal/default Kconfig values
├── sdkconfig.old               # Previous sdkconfig backup
├── partitions.csv              # Custom partition table (graphics/audio examples only)
├── dependencies.lock           # Locked component versions (after first build)
├── main/
│   ├── main.c / main.cpp       # Application entry point (`app_main`)
│   ├── CMakeLists.txt          # Component registration for `main`
│   ├── idf_component.yml       # Component Manager dependencies
│   └── user_config.h           # Board pin definitions and feature toggles
└── components/                 # Local/vendored components (BSP, UI, apps)
    ├── *_bsp/                  # Board-support components (i2c_bsp, lcd_bl_pwm_bsp, ...)
    ├── user_app/               # High-level application logic
    ├── ui_bsp/                 # GUI Guider generated UI
    ├── SensorLib/              # Sensor abstraction library
    └── codec_board/            # Audio codec board wrapper
```

Built examples also contain a `build/` directory and/or `managed_components/` directory.  `managed_components/` holds components fetched automatically by the Component Manager (e.g. `lvgl__lvgl`, `espressif__esp_lcd_axs15231b`).

### Key configuration files

| File | Role |
|---|---|
| `CMakeLists.txt` | Standard ESP-IDF project CMake boilerplate.  Declares `cmake_minimum_required`, includes `${IDF_PATH}/tools/cmake/project.cmake`, and calls `project(<name>)`. |
| `main/CMakeLists.txt` | Registers the `main` component via `idf_component_register(SRCS ... INCLUDE_DIRS ...)`. |
| `sdkconfig.defaults` | Contains the minimal Kconfig values checked into version control (target, flash size, PSRAM, LVGL options, FreeRTOS tick rate, etc.). |
| `sdkconfig` | Generated full configuration.  Do not edit directly; run `idf.py menuconfig` or modify `sdkconfig.defaults`. |
| `partitions.csv` | Custom partition table used by graphics/audio examples: `nvs`, `phy_init`, `factory` (8 MB app). |
| `main/idf_component.yml` | Declares Component Manager dependencies (IDF version, `lvgl/lvgl`, `esp_lcd_axs15231b`, `esp_io_expander_tca9554`, etc.). |
| `dependencies.lock` | Lock file produced after component resolution. |
| `main/user_config.h` | **Single source of truth** for pin mappings, screen resolution, I2C addresses, and build-time feature flags. |

---

## 4. Code Organization

### 4.1 `main/` — entry point and board configuration

- `main.c` / `main.cpp` implements `app_main()`.
- For LVGL examples it also contains the LVGL porting layer: QSPI initialization, display flush callback, touch read callback, tick timer, and the LVGL handler task.
- `user_config.h` defines pins, display resolution, rotation mode, I2C addresses, and LVGL task parameters.

### 4.2 `components/` — BSP / application components

The repository follows a **BSP-style component model**:

| Component type | Naming pattern | Responsibility |
|---|---|---|
| Board support | `<peripheral>_bsp.c/.h` | Low-level peripheral wrappers: `i2c_bsp`, `lcd_bl_pwm_bsp`, `button_bsp`, `adc_bsp`, `sdcard_bsp`, `audio_bsp`, `esp_wifi_bsp`, `ble_scan_bsp` |
| Application | `user_app.cpp/.c`, `user_app.h` | High-level application logic and FreeRTOS tasks |
| UI | `ui_bsp/` | GUI Guider generated screens, fonts, images, and setup code |
| Sensors | `SensorLib/` | Arduino-style sensor library for PCF85063, QMI8658, touch, etc. |
| Audio | `codec_board/` | Espressif codec board abstraction and I2S setup |

### 4.3 `managed_components/`

Downloaded automatically by the ESP-IDF Component Manager.  Do not edit manually; versions are controlled via `idf_component.yml` and `dependencies.lock`.

### 4.4 Include and linkage conventions

- Headers use standard include guards.
- C++ components wrap C APIs with `extern "C" { ... }` where needed.
- Common public globals declared in `user_app.h` or BSP headers include queues such as `app_touch_data_queue`, event groups such as `wifi_even_`, `ble_even_`, `boot_groups`, `pwr_groups`, and event handles such as `sdcard_even_`.

---

## 5. Build Process

### 5.1 Prerequisites

- ESP-IDF installed and environment sourced (`$IDF_PATH` must be set, `idf.py` available).
- Target is always `esp32s3`.

### 5.2 Standard build workflow

Every example is built independently:

```bash
cd <Example>
idf.py set-target esp32s3
idf.py build
idf.py flash
idf.py monitor
```

Or combined:

```bash
cd <Example>
idf.py set-target esp32s3
idf.py build flash monitor
```

### 5.3 Configuration changes

To change Kconfig options:

```bash
cd <Example>
idf.py menuconfig
# save, then rebuild
idf.py build
```

To update the checked-in defaults:

```bash
idf.py save-defconfig
```

### 5.4 Component Manager

If `managed_components/` or `dependencies.lock` is missing, the Component Manager downloads dependencies on the first build.  To force re-resolution:

```bash
idf.py reconfigure
# or remove managed_components/ and dependencies.lock, then build
```

### 5.5 Observed build inconsistencies

A few examples have minor inconsistencies that do not affect building but are worth noting when copying or renaming projects:

- `06_WIFI_STA/CMakeLists.txt` declares `project(06_WIFI_Test)` instead of `06_WIFI_STA`.
- `09_LVGL_V8_Test/CMakeLists.txt` declares `project(12_LVGL_Test)`.
- `10_LVGL_V9_Test/CMakeLists.txt` uses `cmake_minimum_required(VERSION 3.5)` while most others use `3.16`.

---

## 6. Runtime Architecture

### 6.1 FreeRTOS task model

- Tasks are typically created with `xTaskCreatePinnedToCore(..., 0)` or `(..., 1)` to pin them to a specific CPU core.
- LVGL examples run `lv_timer_handler()` in a dedicated FreeRTOS task protected by a mutex (`lvgl_mux`).
- Other peripherals (backlight PWM, audio, sensor polling, Wi-Fi/BLE) run in their own tasks or callbacks.

### 6.2 LVGL porting pattern

Used by all graphics examples (`07`–`11`):

1. Initialize the QSPI bus and create an `esp_lcd_panel_io_handle_t` for the AXS15231B panel.
2. Install the AXS15231B panel driver (`esp_lcd_new_panel_axs15231b`).
3. Allocate a full-screen render buffer in PSRAM plus a smaller DMA-capable transfer buffer.
4. Register `flush_cb` and, for v8, set `disp_drv.full_refresh = 1` (v9 uses `LV_DISPLAY_RENDER_MODE_FULL`).
5. Create an `esp_timer` that calls `lv_tick_inc()` periodically.
6. Run `lv_timer_handler()` in a dedicated task guarded by a mutex.
7. Call the application entry point (`lv_demo_widgets()` or `user_app_init()`) once while holding the LVGL mutex.

### 6.3 Touch input

Touch coordinates are read by sending an 11-byte command (`0xb5 0xab 0xa5 0x5a ... 0x0e`) over the touch I2C bus and parsing the response.  The raw X/Y values are then mapped to the display coordinate system in the touch read callback.  The mapping differs depending on whether software 90° rotation is enabled (`Rotated == USER_DISP_ROT_90`).

### 6.4 Display parameters

- Native panel resolution: **172 × 640** (horizontal × vertical in panel orientation).
- Software rotation to **640 × 172** is selectable via `#define Rotated USER_DISP_ROT_90` in `user_config.h`.
- Color format: RGB565, 16 bpp.

---

## 7. Development Conventions

### 7.1 Languages

- Examples are written in **C** or **C++** depending on complexity.
- C++ is used for UI-heavy examples (`07`, `08`, `09`, `11`); C is used for simpler peripheral demos and for the LVGL v9 example (`10`).

### 7.2 Coding style

- Indentation is **not uniform** across examples: 2-space, 4-space, and tab indentation all appear.
- Braces vary between K&R and Allman styles.
- Most identifiers, log tags, and comments are in English; occasional comments are in Chinese.
- Error handling heavily uses `ESP_ERROR_CHECK(...)` and `ESP_ERROR_CHECK_WITHOUT_ABORT(...)`.

### 7.3 Naming patterns

- Pin macros: `EXAMPLE_PIN_NUM_LCD_*`, `Touch_SCL_NUM`, `Touch_SDA_NUM`, `EXAMPLE_PIN_NUM_BK_LIGHT`, etc.
- BSP files: `<peripheral>_bsp.c/.h`.
- Application files: `user_app.cpp/.c`, `user_app.h`.
- Example tasks: `example_lvgl_port_task`, `example_backlight_loop_task`, etc.

### 7.4 Board-specific configuration

`main/user_config.h` is the authoritative place for:

- QSPI LCD pins and reset/backlight pins.
- Touch I2C pins, reset, interrupt, and I2C address.
- RTC / general-purpose I2C pins.
- Display resolution and buffer sizes.
- Software rotation mode.
- Build-time feature flags (e.g. `Backlight_Testing`).

### 7.5 Dependency models

Two different dependency models coexist:

1. **Component Manager managed** — `10_LVGL_V9_Test` declares `lvgl/lvgl`, `esp_lcd_axs15231b`, etc. in `idf_component.yml` and relies on `managed_components/`.
2. **Vendored** — `11_FactoryProgram` and several others vendor `lvgl`, `SensorLib`, and `codec_board` directly under `components/`.

Do not mix these models inside a single example without understanding the consequences.

---

## 8. Testing and Validation

### 8.1 Automated tests

There are **no automated unit tests, integration tests, or CI workflows** inside `Examples/ESP-IDF`.  Validation is performed by building and running the firmware on the physical board.

### 8.2 Manual validation workflow

For each example:

1. `idf.py build` — verify a clean build.
2. `idf.py flash` — flash to the ESP32-S3-Touch-LCD-3.49 board.
3. `idf.py monitor` — observe serial output and interact with the board.
4. Exercise the example-specific functionality (press buttons, touch the screen, insert SD card, scan Wi-Fi, etc.).

### 8.3 What to watch for

- **Pinout mismatch:** If the display stays black or touch does not respond, confirm whether the example targets V1 or V2 hardware by checking `user_config.h`.
- **LVGL version mismatch:** Code written for LVGL v8 will not compile or run on the LVGL v9 example without porting.
- **PSRAM and partition table:** Graphics/audio examples require the custom `partitions.csv` and PSRAM enabled in `sdkconfig.defaults`.
- **Kconfig drift:** Older examples were created against ESP-IDF 5.3.2/5.4.2.  When building with newer IDF versions, some Kconfig defaults may shift; review `sdkconfig` after `idf.py set-target esp32s3`.

---

## 9. Deployment

Deployment is manual and per-example:

```bash
cd <Example>
idf.py set-target esp32s3
idf.py build flash monitor
```

There are no custom flash scripts, OTA update scripts, or release packaging scripts.  Release artifacts are the `.bin` files produced under `build/`:

- `build/<project>.bin`
- `build/bootloader/bootloader.bin`
- `build/partition_table/partition-table.bin`

Typical flash layout (example `10_LVGL_V9_Test`):

| Offset | File |
|---|---|
| `0x0000` | `bootloader/bootloader.bin` |
| `0x8000` | `partition_table/partition-table.bin` |
| `0x10000` | `<project>.bin` |

---

## 10. Security Considerations

- **No secure boot or flash encryption** is configured in the checked-in `sdkconfig.defaults`.
- Wi-Fi examples (`05_WIFI_AP`, `06_WIFI_STA`, `11_FactoryProgram`) use plaintext credentials or open AP modes where applicable.  Do not commit real network passwords.
- Examples are intended for **hardware evaluation and demonstration**; they are not hardened production firmware.
- The `factory` partition is 8 MB, leaving no separate OTA partition.  Over-the-air updates are not demonstrated.
- `ESP_ERROR_CHECK(...)` aborts on failure; for production code consider graceful error recovery instead.

---

## 11. Common Pitfalls

1. **Hardware revision confusion:** `10_LVGL_V9_Test` currently uses V2 pins while most other examples use V1 pins.  Copying `user_config.h` between examples without checking this will break display or touch.
2. **LVGL v8 vs v9:** The two APIs are incompatible.  Display/touch code from `07`–`09`/`11` cannot be dropped into `10` without porting.
3. **Component Manager vs vendored LVGL:** Modifying files under `managed_components/` is pointless because they are regenerated.  Use `components/lvgl/` in vendored examples instead.
4. **Untracked work-in-progress:** `10_LVGL_V9_Test/components/` contains stub components (`alarm_buzzer`, `axs15231b_display`, `pcf85063_rtc`, `touch_panel`, `ui_calendar`, `wifi_sync`) with `TODO` comments.  They are not yet wired into `main.c`.
5. **Mixed indentation and style:** Expect inconsistent formatting; prefer minimal, localized changes and match the surrounding style.

---

## 12. Useful Reference Links

- ESP-IDF build system: https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
- Waveshare product wiki (English): https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-3.49
- Waveshare product wiki (中文): https://www.waveshare.net/wiki/ESP32-S3-Touch-LCD-3.49

---

*This AGENTS.md was created by summarizing the actual files in `Examples/ESP-IDF`.  Prior to this file, no agent-specific guidance existed in this directory.*
