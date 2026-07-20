#ifndef USER_CONFIG_H
#define USER_CONFIG_H

#include "driver/gpio.h"

// SPI host
#define SDSPI_HOST SPI2_HOST
#define LCD_HOST   SPI3_HOST

// ESP32-S3-Touch-LCD-3.49 pinout (verified on hardware against the vendor
// factory program: LCD data 9-14, RST=21 direct, backlight=GPIO8 PWM).
// NOTE: the earlier "V2" values (BK=42, RST=-1 via TCA9554 EXIO5) left the
// backlight off -> black screen on this board.
#define EXAMPLE_PIN_NUM_LCD_CS        (GPIO_NUM_9)
#define EXAMPLE_PIN_NUM_LCD_PCLK      (GPIO_NUM_10)
#define EXAMPLE_PIN_NUM_LCD_DATA0     (GPIO_NUM_11)
#define EXAMPLE_PIN_NUM_LCD_DATA1     (GPIO_NUM_12)
#define EXAMPLE_PIN_NUM_LCD_DATA2     (GPIO_NUM_13)
#define EXAMPLE_PIN_NUM_LCD_DATA3     (GPIO_NUM_14)
#define EXAMPLE_PIN_NUM_LCD_RST       (GPIO_NUM_21)  /* direct reset, as in factory program */
#define EXAMPLE_PIN_NUM_LCD_TE        (-1)           /* not used */
#define EXAMPLE_PIN_NUM_BK_LIGHT      (GPIO_NUM_8)   /* PWM backlight, as in factory program */

// Display parameters
#define EXAMPLE_LCD_H_RES             172
#define EXAMPLE_LCD_V_RES             640
#define LVGL_DMA_BUFF_LEN             (EXAMPLE_LCD_H_RES * 64 * 2)
#define LVGL_SPIRAM_BUFF_LEN          (EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * 2)

// Rotation (software implemented)
#define USER_DISP_ROT_90              1
#define USER_DISP_ROT_NONO            0
/* ROT_90 path is always compiled so orientation can switch at runtime via
   lv_display_set_rotation(); boot stays at rotation 0 (portrait) and the
   IMU orientation task rotates on demand. */
#define Rotated                       USER_DISP_ROT_90

// Backlight test enable
#define Backlight_Testing             0

// Touch panel I2C (AXS15231B built-in touch)
// V2 keeps TP SDA/SCL on GPIO17/18, same as V1.
#define Touch_SDA_NUM                 (GPIO_NUM_17)
#define Touch_SCL_NUM                 (GPIO_NUM_18)
#define TP_INT_NUM                    (-1)           /* EXIO0 on TCA9554, not used here */
#define DISP_TOUCH_ADDR               0x3B
#define EXAMPLE_PIN_NUM_TOUCH_RST     (-1)
#define EXAMPLE_PIN_NUM_TOUCH_INT     (TP_INT_NUM)

// RTC PCF85063 I2C
#define RTC_SDA_NUM                   (GPIO_NUM_47)
#define RTC_SCL_NUM                   (GPIO_NUM_48)

#endif
