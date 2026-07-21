
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_err.h"
#include "esp_log.h"

#include "lvgl.h"
#include "esp_lcd_axs15231b.h"
#include "user_config.h"
#include "i2c_bsp.h"
#include "lcd_bl_pwm_bsp.h"
#include "ui_calendar.h"
#include "ui_styles.h"
#include "pcf85063.h"
#include "usb_screenshot.h"
#include "battery_bsp.h"
#include "qmi8658_imu.h"
#include "wifi_sync.h"
#include "nvs_flash.h"
#include <math.h>

/* Demo courses for hardware GUI validation - use only characters in built-in font */
static const course_t s_demo_courses[] = {
    {
        .id = "course_001",
        .name = "数学",
        .day_of_week = 1,
        .start_time = "08:30",
        .end_time = "10:00",
        .teacher = "老师",
        .location = "201教室",
        .color = "#FF6B6B",
        .remind_before = 15,
    },
    {
        .id = "course_002",
        .name = "英语",
        .day_of_week = 1,
        .start_time = "10:30",
        .end_time = "12:00",
        .teacher = "老师",
        .location = "202教室",
        .color = "#4ECDC4",
        .remind_before = 10,
    },
    {
        .id = "course_003",
        .name = "科学",
        .day_of_week = 1,
        .start_time = "14:00",
        .end_time = "15:30",
        .teacher = "老师",
        .location = "实验室",
        .color = "#FFE66D",
        .remind_before = 5,
    },
    {
        .id = "course_004",
        .name = "美术",
        .day_of_week = 1,
        .start_time = "16:00",
        .end_time = "17:30",
        .teacher = "老师",
        .location = "美术室",
        .color = "#AA96DA",
        .remind_before = 10,
    },
};

static const int s_demo_course_count = sizeof(s_demo_courses) / sizeof(s_demo_courses[0]);

/* Server-synced courses (fallback: demo data until the first fetch) */
#define MAX_COURSES 16
static course_t s_courses[MAX_COURSES];
static uint32_t s_courses_ver = 0;
static volatile bool s_sync_requested = false;

static const char *TAG = "example";

static void on_sync_request(void)
{
    s_sync_requested = true;
}

static const char *kid_display_name(const char *id)
{
    return (id && strcmp(id, "zhuangzhuang") == 0) ? "壮壮" : "美熹";
}

static void on_kid_switch(void)
{
    wifi_sync_toggle_kid();
    ESP_LOGI(TAG, "kid -> %s", wifi_sync_get_kid());
    ui_set_kid_label(kid_display_name(wifi_sync_get_kid()));
    s_sync_requested = true;   /* refresh courses for the new kid */
}


static SemaphoreHandle_t lvgl_mux = NULL;   
static SemaphoreHandle_t flush_done_semaphore = NULL; 
uint8_t *lvgl_dest = NULL;
static lv_display_t *s_disp = NULL;

static uint16_t *trans_buf_1;

/* Frame buffer for screenshot service */
static uint8_t *s_frame_buffer = NULL;
static uint16_t s_fb_width = 0;
static uint16_t s_fb_height = 0;

#define LCD_BIT_PER_PIXEL 16
#define BYTES_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565))
#define BUFF_SIZE (EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * BYTES_PER_PIXEL)

#define LVGL_TICK_PERIOD_MS    5
#define LVGL_TASK_MAX_DELAY_MS 500
#define LVGL_TASK_MIN_DELAY_MS 10
#define LVGL_TASK_STACK_SIZE   (8 * 1024)
#define LVGL_TASK_PRIORITY     2


uint16_t *get_frame_buffer(uint16_t *width, uint16_t *height)
{
    *width = s_fb_width;
    *height = s_fb_height;
    return (uint16_t *)s_frame_buffer;
}

static void example_backlight_loop_task(void *arg);

static bool example_lvgl_lock(int timeout_ms);
static void example_lvgl_unlock(void);

/* Force a synchronous LVGL refresh (used by the screenshot hook) */
static void screenshot_refresh(void)
{
    lv_refr_now(NULL);
}

/* Poll the accelerometer and rotate the UI when the device orientation
   changes. Four directions, gravity decides the dominant axis:
     y > 0 -> ROTATION_90,  y < 0 -> ROTATION_270,
     x > 0 -> ROTATION_0,   x < 0 -> ROTATION_180
   (y>0 == landscape-90 verified on hardware; the x sign convention may
   still need flipping after physical testing.)
   Debounce: switch only after 5 consecutive readings of the new direction. */
#define ORIENT_DEBOUNCE_COUNT 5

static const char *s_orient_names[] = {"0", "90", "180", "270"};

static void imu_orientation_task(void *arg)
{
    lv_display_rotation_t applied = LV_DISPLAY_ROTATION_0;
    lv_display_rotation_t candidate = LV_DISPLAY_ROTATION_0;
    int stable = 0;

    for (;;) {
        float x = 0, y = 0, z = 0;
        if (imu_read_accel(&x, &y, &z) == ESP_OK) {
            lv_display_rotation_t now;
            if (fabsf(y) > fabsf(x)) {
                now = (y > 0) ? LV_DISPLAY_ROTATION_90 : LV_DISPLAY_ROTATION_270;
            } else {
                now = (x > 0) ? LV_DISPLAY_ROTATION_0 : LV_DISPLAY_ROTATION_180;
            }

            if (now == candidate) {
                if (stable < ORIENT_DEBOUNCE_COUNT) stable++;
            } else {
                candidate = now;
                stable = 1;
            }

            if (stable >= ORIENT_DEBOUNCE_COUNT && candidate != applied) {
                applied = candidate;
                bool landscape = (applied == LV_DISPLAY_ROTATION_90 ||
                                  applied == LV_DISPLAY_ROTATION_270);
                ESP_LOGI(TAG, "Orientation -> %s (x=%.2f y=%.2f z=%.2f)",
                         s_orient_names[applied], x, y, z);
                if (example_lvgl_lock(-1)) {
                    lv_display_set_rotation(s_disp, applied);
                    ui_set_orientation(landscape);
                    /* Screenshot dims follow the logical resolution */
                    usb_screenshot_register_fb((uint16_t *)s_frame_buffer,
                                               landscape ? EXAMPLE_LCD_V_RES : EXAMPLE_LCD_H_RES,
                                               landscape ? EXAMPLE_LCD_H_RES : EXAMPLE_LCD_V_RES);
                    example_lvgl_unlock();
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

static void example_backlight_loop_task(void *arg);


static const axs15231b_lcd_init_cmd_t lcd_init_cmds[] = 
{
  	{0x11, (uint8_t []){0x00}, 0, 100},
    {0x29, (uint8_t []){0x00}, 0, 100},
};

static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
  	BaseType_t high_task_awoken = pdFALSE;
  	xSemaphoreGiveFromISR(flush_done_semaphore, &high_task_awoken);
  	return false;
}

static void example_lvgl_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * color_p)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);
    lv_draw_sw_rgb565_swap(color_p, lv_area_get_width(area) * lv_area_get_height(area));
#if (Rotated == USER_DISP_ROT_90)
    lv_display_rotation_t rotation = lv_display_get_rotation(disp);
    lv_area_t rotated_area;
    if(rotation != LV_DISPLAY_ROTATION_0)
    {
        lv_color_format_t cf = lv_display_get_color_format(disp);
        /*Calculate the position of the rotated area*/
        rotated_area = *area;
        lv_display_rotate_area(disp, &rotated_area);
        /*Calculate the source stride (bytes in a line) from the width of the area*/
        uint32_t src_stride = lv_draw_buf_width_to_stride(lv_area_get_width(area), cf);
        /*Calculate the stride of the destination (rotated) area too*/
        uint32_t dest_stride = lv_draw_buf_width_to_stride(lv_area_get_width(&rotated_area), cf);
        /*Have a buffer to store the rotated area and perform the rotation*/
        
        int32_t src_w = lv_area_get_width(area);
        int32_t src_h = lv_area_get_height(area);
        lv_draw_sw_rotate(color_p, lvgl_dest, src_w, src_h, src_stride, dest_stride, rotation, cf);
        /*Use the rotated area and rotated buffer from now on*/
        area = &rotated_area;
    }

    const int flush_coun = (LVGL_SPIRAM_BUFF_LEN / LVGL_DMA_BUFF_LEN);
    const int offgap = (EXAMPLE_LCD_V_RES / flush_coun);
    const int dmalen = (LVGL_DMA_BUFF_LEN / 2);
    int offsetx1 = 0;
    int offsety1 = 0;
    int offsetx2 = EXAMPLE_LCD_H_RES;
    int offsety2 = offgap;

    /* rotation==0 renders straight from color_p; rotated frames live in lvgl_dest */
    uint16_t *map = (rotation != LV_DISPLAY_ROTATION_0) ? (uint16_t *)lvgl_dest
                                                        : (uint16_t *)color_p;
    xSemaphoreGive(flush_done_semaphore);
    for(int i = 0; i<flush_coun; i++)
    {
        xSemaphoreTake(flush_done_semaphore,portMAX_DELAY);
        memcpy(trans_buf_1,map,LVGL_DMA_BUFF_LEN);
        esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2, offsety2, trans_buf_1);
        offsety1 += offgap;
        offsety2 += offgap;
        map += dmalen;
    }
    xSemaphoreTake(flush_done_semaphore,portMAX_DELAY);
    lv_disp_flush_ready(disp);
#else
    const int flush_coun = (LVGL_SPIRAM_BUFF_LEN / LVGL_DMA_BUFF_LEN);
    const int offgap = (EXAMPLE_LCD_V_RES / flush_coun);
    const int dmalen = (LVGL_DMA_BUFF_LEN / 2);
    int offsetx1 = 0;
    int offsety1 = 0;
    int offsetx2 = EXAMPLE_LCD_H_RES;
    int offsety2 = offgap;

    uint16_t *map = (uint16_t *)color_p;
    xSemaphoreGive(flush_done_semaphore);
    for(int i = 0; i<flush_coun; i++)
    {
        xSemaphoreTake(flush_done_semaphore,portMAX_DELAY);
        memcpy(trans_buf_1,map,LVGL_DMA_BUFF_LEN);
        esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2, offsety2, trans_buf_1);
        offsety1 += offgap;
        offsety2 += offgap;
        map += dmalen;
    }
    xSemaphoreTake(flush_done_semaphore,portMAX_DELAY);
    lv_disp_flush_ready(disp);
#endif
}

static void TouchInputReadCallback(lv_indev_t * indev, lv_indev_data_t *indevData)
{
    uint8_t read_touchpad_cmd[11] = {0xb5, 0xab, 0xa5, 0x5a, 0x0, 0x0, 0x0, 0x0e,0x0, 0x0, 0x0};
    uint8_t buff[32] = {0};
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_write_read_dev(disp_touch_dev_handle,read_touchpad_cmd,11,buff,32));
    uint16_t pointX;
    uint16_t pointY;
    pointX = (((uint16_t)buff[2] & 0x0f) << 8) | (uint16_t)buff[3];
    pointY = (((uint16_t)buff[4] & 0x0f) << 8) | (uint16_t)buff[5];
    //ESP_LOGI("Touch","%d,%d",buff[0],buff[1]);
    if (buff[1]>0 && buff[1]<5)
    {
        indevData->state = LV_INDEV_STATE_PRESSED;
        /* Always report in the physical (portrait) frame — LVGL 9 rotates
           indev points itself per lv_display_set_rotation()
           (see lv_display_rotate_point in lv_indev.c). */
        if(pointX > EXAMPLE_LCD_V_RES) pointX = EXAMPLE_LCD_V_RES;
        if(pointY > EXAMPLE_LCD_H_RES) pointY = EXAMPLE_LCD_H_RES;
        indevData->point.x = pointY;
        indevData->point.y = (EXAMPLE_LCD_V_RES-pointX);
    }
    else 
    {
        indevData->state = LV_INDEV_STATE_RELEASED;
    }
}

static void example_increase_lvgl_tick(void *arg)
{
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

static bool example_lvgl_lock(int timeout_ms)
{
    assert(lvgl_mux && "bsp_display_start must be called first");

    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(lvgl_mux, timeout_ticks) == pdTRUE;
}

static void example_lvgl_unlock(void)
{
    assert(lvgl_mux && "bsp_display_start must be called first");
    xSemaphoreGive(lvgl_mux);
}

static void example_lvgl_port_task(void *arg)
{
    uint32_t task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
    for(;;)
    {
        // Lock the mutex due to the LVGL APIs are not thread-safe
        if (example_lvgl_lock(-1))
        {
            task_delay_ms = lv_timer_handler();
            // Release the mutex
            example_lvgl_unlock();
        }
        if (task_delay_ms > LVGL_TASK_MAX_DELAY_MS)
        {
            task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
        }
        else if (task_delay_ms < LVGL_TASK_MIN_DELAY_MS)
        {
            task_delay_ms = LVGL_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

void app_main(void)
{
    /* NVS (required by the Wi-Fi driver) */
    {
        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_ERROR_CHECK(nvs_flash_erase());
            ret = nvs_flash_init();
        }
        ESP_ERROR_CHECK(ret);
    }

    flush_done_semaphore = xSemaphoreCreateBinary();
    assert(flush_done_semaphore);
    touch_i2c_master_Init();
    ESP_LOGI(TAG, "Initialize SPI bus");
#if (EXAMPLE_PIN_NUM_LCD_RST >= 0)
	gpio_config_t gpio_conf = {};
    gpio_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_conf.mode = GPIO_MODE_OUTPUT;
    gpio_conf.pin_bit_mask = ((uint64_t)0x01<<EXAMPLE_PIN_NUM_LCD_RST);
    gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&gpio_conf));
#endif

    spi_bus_config_t buscfg = {};
    buscfg.sclk_io_num =  EXAMPLE_PIN_NUM_LCD_PCLK;  
    buscfg.data0_io_num = EXAMPLE_PIN_NUM_LCD_DATA0;            
    buscfg.data1_io_num = EXAMPLE_PIN_NUM_LCD_DATA1;             
    buscfg.data2_io_num = EXAMPLE_PIN_NUM_LCD_DATA2;
    buscfg.data3_io_num = EXAMPLE_PIN_NUM_LCD_DATA3;
    buscfg.max_transfer_sz = LVGL_DMA_BUFF_LEN;
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));  
    
	ESP_LOGI(TAG, "Install panel IO");
	esp_lcd_panel_io_handle_t panel_io = NULL;
    esp_lcd_panel_handle_t panel = NULL;
    
    esp_lcd_panel_io_spi_config_t io_config = {};
		io_config.cs_gpio_num = EXAMPLE_PIN_NUM_LCD_CS;                 
        io_config.dc_gpio_num = -1;          
        io_config.spi_mode = 3;              
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;    
        io_config.on_color_trans_done = example_notify_lvgl_flush_ready; 
        //io_config.user_ctx = &disp_drv,         
        io_config.lcd_cmd_bits = 32;         
        io_config.lcd_param_bits = 8;        
        io_config.flags.quad_mode = true;                         
	ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_HOST, &io_config, &panel_io));
    
	axs15231b_vendor_config_t vendor_config = {};
    vendor_config.flags.use_qspi_interface = 1;
    vendor_config.init_cmds = lcd_init_cmds;
    vendor_config.init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]);
    
    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = -1;
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_config.bits_per_pixel = LCD_BIT_PER_PIXEL;
    panel_config.vendor_config = &vendor_config;
    
    ESP_LOGI(TAG, "Install panel driver");
    ESP_ERROR_CHECK(esp_lcd_new_panel_axs15231b(panel_io, &panel_config, &panel));

    /* Cold-boot timing: the panel needs time to power up after a cold
       power-on. Demo 8 reaches panel init later (I2C/SensorLib init first)
       and always works; kids-calendar used to get here in <100ms and the
       init commands were lost -> black screen with a running app. */
    vTaskDelay(pdMS_TO_TICKS(300));
#if (EXAMPLE_PIN_NUM_LCD_RST >= 0)
	ESP_ERROR_CHECK(gpio_set_level(EXAMPLE_PIN_NUM_LCD_RST,1));
    vTaskDelay(pdMS_TO_TICKS(30));
    ESP_ERROR_CHECK(gpio_set_level(EXAMPLE_PIN_NUM_LCD_RST,0));
    vTaskDelay(pdMS_TO_TICKS(250));
    ESP_ERROR_CHECK(gpio_set_level(EXAMPLE_PIN_NUM_LCD_RST,1));
    vTaskDelay(pdMS_TO_TICKS(30));
#else
    vTaskDelay(pdMS_TO_TICKS(100));
#endif
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));

    /*lvgl port*/
    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
    lv_display_t * disp = lv_display_create(EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES);  /* 以水平和垂直分辨率（像素）进行基本初始化 */
    lv_display_set_flush_cb(disp, example_lvgl_flush_cb);                           /* 设置刷新回调函数以绘制到显示屏 */
    
    uint8_t *buffer_1 = NULL;
    uint8_t *buffer_2 = NULL;
    buffer_1 = (uint8_t *)heap_caps_malloc(BUFF_SIZE, MALLOC_CAP_SPIRAM);
    assert(buffer_1);
    buffer_2 = (uint8_t *)heap_caps_malloc(BUFF_SIZE, MALLOC_CAP_SPIRAM);
    assert(buffer_2);
	trans_buf_1 = (uint16_t *)heap_caps_malloc(LVGL_DMA_BUFF_LEN, MALLOC_CAP_DMA);
	assert(trans_buf_1);
    lv_display_set_buffers(disp, buffer_1, buffer_2, BUFF_SIZE, LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_user_data(disp, panel);

    /* Save frame buffer for screenshot service */
    s_frame_buffer = buffer_1;
    s_fb_width = EXAMPLE_LCD_H_RES;
    s_fb_height = EXAMPLE_LCD_V_RES;
#if (Rotated == USER_DISP_ROT_90)
    /* Rotation buffer for runtime orientation switching; boot stays portrait,
       the IMU task calls lv_display_set_rotation() on demand. */
    lvgl_dest = (uint8_t *)heap_caps_malloc(BUFF_SIZE, MALLOC_CAP_SPIRAM); //旋转buf
    assert(lvgl_dest);
#endif
    s_disp = disp;

    /*port indev*/
    lv_indev_t *touch_indev = NULL;
    touch_indev = lv_indev_create();
    lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch_indev, TouchInputReadCallback);

    esp_timer_create_args_t lvgl_tick_timer_args = {};
    lvgl_tick_timer_args.callback = &example_increase_lvgl_tick;
    lvgl_tick_timer_args.name = "lvgl_tick";
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));

    lvgl_mux = xSemaphoreCreateMutex(); //mutex semaphores
    assert(lvgl_mux);
    xTaskCreatePinnedToCore(example_lvgl_port_task, "LVGL", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, NULL,0);
    xTaskCreatePinnedToCore(example_backlight_loop_task, "example_backlight_loop_task", 4 * 1024, NULL, 2, NULL,0); 
    if (example_lvgl_lock(-1)) 
    {   
        ui_init();
        ui_show_course_timeline(s_demo_courses, s_demo_course_count);
        ui_show_month_calendar(2026, 7, 20);

        // Release the mutex
        example_lvgl_unlock();
    }

    /* Register frame buffer for USB screenshot debugging */
    usb_screenshot_register_fb((uint16_t *)s_frame_buffer, s_fb_width, s_fb_height);
    usb_screenshot_set_hooks(example_lvgl_lock, example_lvgl_unlock, screenshot_refresh);

    /* On-demand screenshot: send 'screenshot' over USB serial to trigger */
    usb_screenshot_start_cmd();

    /* Initialize RTC after UI is up */
    ESP_ERROR_CHECK(rtc_init());

    /* Battery voltage measurement (ADC1_CH3 only; no TCA9554 access — those
       writes kill the panel on this board) */
    ESP_ERROR_CHECK_WITHOUT_ABORT(battery_bsp_init());

    /* IMU for auto-rotation (same I2C bus as RTC) */
    if (imu_init() == ESP_OK) {
        xTaskCreatePinnedToCore(imu_orientation_task, "imu_orient", 4096, NULL, 3, NULL, 0);
    } else {
        ESP_LOGW(TAG, "IMU not found, auto-rotation disabled");
    }

    /* Set a default time only if the RTC lost its time (first boot or
       battery change); NTP sync overwrites it once Wi-Fi is up. */
    {
        struct tm rtc_now = {0};
        if (rtc_get_time(&rtc_now) != ESP_OK || rtc_now.tm_year < 126) {
            struct tm default_time = {
                .tm_year = 126, /* 2026 - 1900 */
                .tm_mon = 6,    /* July (0-based) */
                .tm_mday = 20,
                .tm_hour = 12,
                .tm_min = 30,
                .tm_sec = 0,
                .tm_wday = 1,  /* Monday */
            };
            rtc_set_time(&default_time);
        }
    }

    /* Wi-Fi + NTP time sync (credentials via menuconfig) */
    if (wifi_sync_init() == ESP_OK) {
        wifi_sync_start_time_task();
    }

    /* Seed courses from the NVS cache (fast, offline-safe); the background
       course task refreshes from the server when Wi-Fi is up */
    {
        int n = wifi_sync_get_courses(s_courses, MAX_COURSES);
        if (n > 0 && example_lvgl_lock(100)) {
            ui_show_course_timeline(s_courses, n);
            example_lvgl_unlock();
        }
        ui_set_sync_callback(on_sync_request);
        ui_set_kid_switch_callback(on_kid_switch);
        ui_set_kid_label(kid_display_name(wifi_sync_get_kid()));
    }

    /* Main loop: update status bar only when minute changes to avoid flicker */
    int last_min = -1;
    while (1) {
        /* Manual sync requested by tapping the status bar */
        if (s_sync_requested) {
            s_sync_requested = false;
            ESP_LOGI(TAG, "manual course sync...");
            wifi_sync_refresh_courses();
        }

        /* Pick up fresh course data when the version changes */
        uint32_t v = wifi_sync_courses_version();
        if (v != s_courses_ver) {
            s_courses_ver = v;
            int n = wifi_sync_get_courses(s_courses, MAX_COURSES);
            if (n > 0 && example_lvgl_lock(100)) {
                ui_show_course_timeline(s_courses, n);
                example_lvgl_unlock();
            }
        }

        struct tm now;
        if (rtc_get_time(&now) == ESP_OK) {
            if (now.tm_min != last_min) {
                last_min = now.tm_min;
                char date_buf[16];
                char time_buf[16];
                snprintf(date_buf, sizeof(date_buf), "%02d/%02d", now.tm_mon + 1, now.tm_mday);
                snprintf(time_buf, sizeof(time_buf), "%02d:%02d", now.tm_hour, now.tm_min);

                const char *weekday = "--";
                const char *weekday_names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
                if (now.tm_wday >= 0 && now.tm_wday <= 6) {
                    weekday = weekday_names[now.tm_wday];
                }

                if (example_lvgl_lock(100)) {
                    char wifi_text[24] = "WiFi";
                    bool wifi_ok = wifi_sync_get_ssid(wifi_text, sizeof(wifi_text));
                    ui_update_statusbar(date_buf, weekday, time_buf, wifi_ok, wifi_text);
                    ui_update_battery(battery_get_percent(), battery_is_charging());
                    ui_set_current_time(now.tm_hour, now.tm_min);
                    /* Re-render with the freshest source (server if available) */
                    int n = wifi_sync_get_courses(s_courses, MAX_COURSES);
                    if (n > 0) {
                        ui_show_course_timeline(s_courses, n);
                    } else {
                        ui_show_course_timeline(s_demo_courses, s_demo_course_count);
                    }
                    example_lvgl_unlock();
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void example_backlight_loop_task(void *arg)
{
    for(;;)
    {
#if  (Backlight_Testing == true)
        vTaskDelay(pdMS_TO_TICKS(1500));
        setUpduty(LCD_PWM_MODE_255);
        vTaskDelay(pdMS_TO_TICKS(1500));
        setUpduty(LCD_PWM_MODE_175);
        vTaskDelay(pdMS_TO_TICKS(1500));
        setUpduty(LCD_PWM_MODE_125);
        vTaskDelay(pdMS_TO_TICKS(1500));
        setUpduty(LCD_PWM_MODE_0);
#else
        vTaskDelay(pdMS_TO_TICKS(2000));
#endif
    }
}
