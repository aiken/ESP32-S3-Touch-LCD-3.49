#ifndef UI_CALENDAR_H
#define UI_CALENDAR_H

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Calendar datetime representation
 */
typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
} ui_calendar_datetime_t;

/**
 * @brief Calendar event entry
 */
typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    char title[64];
} ui_calendar_event_t;

/**
 * @brief Initialize the calendar UI
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ui_calendar_init(void);

/**
 * @brief Deinitialize the calendar UI
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ui_calendar_deinit(void);

/**
 * @brief Update displayed datetime
 *
 * @param datetime Pointer to datetime
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ui_calendar_set_datetime(const ui_calendar_datetime_t *datetime);

/**
 * @brief Add an event to the calendar
 *
 * @param event Pointer to event data
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ui_calendar_add_event(const ui_calendar_event_t *event);

/**
 * @brief Remove event(s) on a specific date
 *
 * @param year Event year
 * @param month Event month
 * @param day Event day
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ui_calendar_remove_event(uint16_t year, uint8_t month, uint8_t day);

/**
 * @brief Show the calendar screen
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ui_calendar_show(void);

/**
 * @brief Hide the calendar screen
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ui_calendar_hide(void);

#ifdef __cplusplus
}
#endif

#endif
