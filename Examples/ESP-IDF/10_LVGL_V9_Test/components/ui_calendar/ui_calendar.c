#include "ui_calendar.h"
#include "esp_log.h"
#include "esp_check.h"
#include "lvgl.h"

static const char *TAG = "UI_CALENDAR";

static bool s_inited = false;

esp_err_t ui_calendar_init(void)
{
    ESP_LOGI(TAG, "Initializing calendar UI");

    /* TODO: create calendar + clock widgets */
    /* Large widget buffers (> 1 KB) must be allocated from heap */

    s_inited = true;
    return ESP_OK;
}

esp_err_t ui_calendar_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing calendar UI");

    /* TODO: delete LVGL objects */
    s_inited = false;
    return ESP_OK;
}

esp_err_t ui_calendar_set_datetime(const ui_calendar_datetime_t *datetime)
{
    ESP_RETURN_ON_FALSE(s_inited, ESP_ERR_INVALID_STATE, TAG, "calendar UI not initialized");
    ESP_RETURN_ON_FALSE(datetime != NULL, ESP_ERR_INVALID_ARG, TAG, "datetime is NULL");

    ESP_LOGI(TAG, "Set datetime %04u-%02u-%02u %02u:%02u",
             datetime->year, datetime->month, datetime->day,
             datetime->hour, datetime->minute);
    /* TODO: update LVGL labels/calendar */
    return ESP_OK;
}

esp_err_t ui_calendar_add_event(const ui_calendar_event_t *event)
{
    ESP_RETURN_ON_FALSE(s_inited, ESP_ERR_INVALID_STATE, TAG, "calendar UI not initialized");
    ESP_RETURN_ON_FALSE(event != NULL, ESP_ERR_INVALID_ARG, TAG, "event is NULL");

    ESP_LOGI(TAG, "Add event %04u-%02u-%02u: %s",
             event->year, event->month, event->day, event->title);
    /* TODO: add event to calendar highlight list */
    return ESP_OK;
}

esp_err_t ui_calendar_remove_event(uint16_t year, uint8_t month, uint8_t day)
{
    ESP_RETURN_ON_FALSE(s_inited, ESP_ERR_INVALID_STATE, TAG, "calendar UI not initialized");

    ESP_LOGI(TAG, "Remove events on %04u-%02u-%02u", year, month, day);
    /* TODO: remove matching events */
    return ESP_OK;
}

esp_err_t ui_calendar_show(void)
{
    ESP_RETURN_ON_FALSE(s_inited, ESP_ERR_INVALID_STATE, TAG, "calendar UI not initialized");

    ESP_LOGI(TAG, "Show calendar screen");
    /* TODO: load calendar screen */
    return ESP_OK;
}

esp_err_t ui_calendar_hide(void)
{
    ESP_RETURN_ON_FALSE(s_inited, ESP_ERR_INVALID_STATE, TAG, "calendar UI not initialized");

    ESP_LOGI(TAG, "Hide calendar screen");
    /* TODO: hide calendar screen */
    return ESP_OK;
}
