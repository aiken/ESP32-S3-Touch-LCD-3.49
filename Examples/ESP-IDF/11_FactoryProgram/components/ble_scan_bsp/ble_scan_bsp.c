#include <stdio.h>
#include "ble_scan_bsp.h"
#include "esp_log.h"
#include "freertos/event_groups.h"

#define GATTC_TAG "GATTC_DEMO"

EventGroupHandle_t ble_even_ = NULL;
int ble_scan_apNum = 0;

static void ble_scan_resources_init(void)
{
    if (ble_even_ == NULL) {
        ble_even_ = xEventGroupCreate();
    }
}

void ble_scan_prepare(void)
{
    ble_scan_resources_init();
}

void ble_stack_init(void)
{
    ble_scan_resources_init();
    ESP_LOGW(GATTC_TAG, "BLE stack stub: no scan performed");
    /* Stub: immediately signal scan completion so UI is not blocked. */
    ble_scan_apNum = 0;
    xEventGroupSetBits(ble_even_, 0x01);
}

void ble_scan_start(void)
{
    /* No-op in stub. */
}

void ble_stack_deinit(void)
{
    /* No-op in stub. */
}
