#ifndef USER_CONFIG_H
#define USER_CONFIG_H

#include "driver/gpio.h"

// SPI host
#define SDSPI_HOST SPI2_HOST
#define LCD_HOST   SPI3_HOST

// ESP32-S3-Touch-LCD-3.49 V2 pinout (PCB Rev2.0)
// LCD QSPI (AXS15231B)
// Note: LCD_RST and TP_INT are routed through the TCA9554 I/O expander (EXIO).
//       For this minimal LVGL v9 port we leave them uncontrolled (-1) and rely
//       on the panel driver's soft reset / polling touch read.
#define EXAMPLE_PIN_NUM_LCD_CS        (GPIO_NUM_9)
#define EXAMPLE_PIN_NUM_LCD_PCLK      (GPIO_NUM_10)
#define EXAMPLE_PIN_NUM_LCD_DATA0     (GPIO_NUM_11)
#define EXAMPLE_PIN_NUM_LCD_DATA1     (GPIO_NUM_12)
#define EXAMPLE_PIN_NUM_LCD_DATA2     (GPIO_NUM_13)
#define EXAMPLE_PIN_NUM_LCD_DATA3     (GPIO_NUM_14)
#define EXAMPLE_PIN_NUM_LCD_RST       (-1)           /* EXIO5 on TCA9554 */
#define EXAMPLE_PIN_NUM_LCD_TE        (GPIO_NUM_21)
#define EXAMPLE_PIN_NUM_BK_LIGHT      (GPIO_NUM_42)  /* direct PWM backlight */

// Display parameters
#define EXAMPLE_LCD_H_RES             172
#define EXAMPLE_LCD_V_RES             640
#define LVGL_DMA_BUFF_LEN             (EXAMPLE_LCD_H_RES * 64 * 2)
#define LVGL_SPIRAM_BUFF_LEN          (EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * 2)

// Rotation (software implemented)
#define USER_DISP_ROT_90              1
#define USER_DISP_ROT_NONO            0
#define Rotated                       USER_DISP_ROT_NONO

// Backlight test enable
#define Backlight_Testing             1

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
