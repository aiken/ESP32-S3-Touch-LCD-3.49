#include "wifi_sync.h"

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "pcf85063.h"

static const char *TAG = "WIFI";

#define WIFI_CONNECTED_BIT  BIT0
#define NTP_RETRY_COUNT     20
#define RESYNC_INTERVAL_MS  (6 * 3600 * 1000)
#define COURSE_SYNC_INTERVAL_MS (30 * 1000)
#define HTTP_BUF_SIZE       (32 * 1024)
#define NVS_NAMESPACE       "cal"
#define NVS_KEY_TODAY       "today"

static EventGroupHandle_t s_wifi_events = NULL;
static bool s_connected = false;

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;
        xEventGroupClearBits(s_wifi_events, WIFI_CONNECTED_BIT);
        ESP_LOGW(TAG, "disconnected, reconnecting...");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        s_connected = true;
        xEventGroupSetBits(s_wifi_events, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

esp_err_t wifi_sync_init(void)
{
    s_wifi_events = xEventGroupCreate();

    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif init failed");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event loop failed");
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "wifi init failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler, NULL, NULL),
                        TAG, "wifi event register failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler, NULL, NULL),
                        TAG, "ip event register failed");

    wifi_config_t wifi_config = {};
    strncpy((char *)wifi_config.sta.ssid, CONFIG_WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, CONFIG_WIFI_PASSWORD, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "set mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG, "set config failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_ps(WIFI_PS_NONE), TAG, "set ps failed");  /* stable link for sync */
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start failed");

    ESP_LOGI(TAG, "connecting to %s ...", CONFIG_WIFI_SSID);
    return ESP_OK;
}

bool wifi_sync_is_connected(void)
{
    return s_connected;
}

bool wifi_sync_get_ssid(char *buf, size_t len)
{
    if (!s_connected || !buf || len == 0) {
        return false;
    }
    wifi_ap_record_t ap = {0};
    const char *ssid = NULL;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK && ap.ssid[0] != '\0') {
        ssid = (const char *)ap.ssid;
    } else {
        ssid = CONFIG_WIFI_SSID;  /* fallback: the configured one */
    }
    strncpy(buf, ssid, len - 1);
    buf[len - 1] = '\0';
    return true;
}

/* ---------------- NTP time sync ---------------- */

static void time_sync_task(void *arg)
{
    /* POSIX TZ sign is inverted: Beijing (UTC+8) -> "CST-8" */
    char tz[16];
    snprintf(tz, sizeof(tz), "CST-%d", CONFIG_TIMEZONE_OFFSET);
    setenv("TZ", tz, 1);
    tzset();

    for (;;) {
        xEventGroupWaitBits(s_wifi_events, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

        ESP_LOGI(TAG, "starting NTP sync (server: %s)", CONFIG_NTP_SERVER);
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, CONFIG_NTP_SERVER);
        esp_sntp_init();

        time_t now = 0;
        struct tm ti = {0};
        bool synced = false;
        for (int i = 0; i < NTP_RETRY_COUNT; i++) {
            time(&now);
            localtime_r(&now, &ti);
            if (ti.tm_year >= 126) {  /* 2026 or later => time is sane */
                synced = true;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        if (synced) {
            if (rtc_set_time(&ti) == ESP_OK) {
                ESP_LOGI(TAG, "RTC synced: %04d-%02d-%02d %02d:%02d:%02d (wday=%d)",
                         ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
                         ti.tm_hour, ti.tm_min, ti.tm_sec, ti.tm_wday);
            } else {
                ESP_LOGE(TAG, "RTC write failed");
            }
        } else {
            ESP_LOGW(TAG, "NTP sync timeout, retry later");
        }

        esp_sntp_stop();
        vTaskDelay(pdMS_TO_TICKS(RESYNC_INTERVAL_MS));
    }
}

/* ---------------- course sync (LAN first, cloud fallback) ---------------- */

static char *http_get(const char *url)
{
    char *buf = heap_caps_malloc(HTTP_BUF_SIZE, MALLOC_CAP_SPIRAM);
    if (!buf) {
        return NULL;
    }

    esp_http_client_config_t cfg = {
        .url = url,
        .timeout_ms = 8000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_http_client_set_header(client, "X-Device-Key", CONFIG_DEVICE_KEY);

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "open failed %s: %s", url, esp_err_to_name(err));
        esp_http_client_cleanup(client);
        free(buf);
        return NULL;
    }

    /* fetch_headers is REQUIRED before status code / body reads,
       otherwise status stays 0 and reads error out */
    int64_t content_len = esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    int total = 0;
    while (total < HTTP_BUF_SIZE - 1) {
        int n = esp_http_client_read(client, buf + total, HTTP_BUF_SIZE - 1 - total);
        if (n < 0) {
            ESP_LOGW(TAG, "read error %d after %d bytes", n, total);
            err = ESP_FAIL;
            break;
        }
        if (n == 0) break;
        total += n;
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (status != 200 || total <= 0) {
        ESP_LOGW(TAG, "GET %s -> %d (%d bytes)", url, status, total);
        free(buf);
        return NULL;
    }
    buf[total] = '\0';
    ESP_LOGI(TAG, "GET %s -> 200 (%d bytes)", url, total);
    return buf;
}

static void copy_str(char *dst, size_t dst_size, const cJSON *obj, const char *key)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive((cJSON *)obj, key);
    if (cJSON_IsString(item) && item->valuestring) {
        strncpy(dst, item->valuestring, dst_size - 1);
        dst[dst_size - 1] = '\0';
    }
}

static int parse_courses_json(const char *json, course_t *out, int max)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return -1;
    }
    const cJSON *courses = cJSON_GetObjectItemCaseSensitive(root, "courses");
    if (!cJSON_IsArray(courses)) {
        cJSON_Delete(root);
        return -1;
    }

    int count = 0;
    const cJSON *item;
    cJSON_ArrayForEach(item, courses) {
        if (count >= max) break;
        course_t *c = &out[count];
        memset(c, 0, sizeof(*c));
        copy_str(c->id, sizeof(c->id), item, "id");
        copy_str(c->name, sizeof(c->name), item, "name");
        copy_str(c->start_time, sizeof(c->start_time), item, "start_time");
        copy_str(c->end_time, sizeof(c->end_time), item, "end_time");
        copy_str(c->teacher, sizeof(c->teacher), item, "teacher");
        copy_str(c->location, sizeof(c->location), item, "location");
        copy_str(c->color, sizeof(c->color), item, "color");
        const cJSON *dow = cJSON_GetObjectItemCaseSensitive(item, "day_of_week");
        const cJSON *rb = cJSON_GetObjectItemCaseSensitive(item, "remind_before");
        c->day_of_week = cJSON_IsNumber(dow) ? (int)dow->valuedouble : 1;
        c->remind_before = cJSON_IsNumber(rb) ? (int)rb->valuedouble : 10;
        if (c->name[0] == '\0') continue;
        count++;
    }
    cJSON_Delete(root);
    return count;
}

/* NVS cache of the last good /api/today payload */
static void cache_save(const char *json)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, NVS_KEY_TODAY, json);
        nvs_commit(h);
        nvs_close(h);
    }
}

static char *cache_load(void)
{
    char *buf = NULL;
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) == ESP_OK) {
        size_t len = 0;
        if (nvs_get_str(h, NVS_KEY_TODAY, NULL, &len) == ESP_OK && len > 1) {
            buf = malloc(len);
            if (buf) {
                nvs_get_str(h, NVS_KEY_TODAY, buf, &len);
            }
        }
        nvs_close(h);
    }
    return buf;
}

/* ---------------- active kid profile (NVS persisted) ---------------- */

#define NVS_KEY_KID  "kid"
static char s_kid[16] = "";

const char *wifi_sync_get_kid(void)
{
    if (s_kid[0] == '\0') {
        nvs_handle_t h;
        if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) == ESP_OK) {
            size_t len = sizeof(s_kid);
            nvs_get_str(h, NVS_KEY_KID, s_kid, &len);
            nvs_close(h);
        }
        if (strcmp(s_kid, "meixi") != 0 && strcmp(s_kid, "zhuangzhuang") != 0) {
            strncpy(s_kid, CONFIG_KID_PROFILE, sizeof(s_kid) - 1);
        }
    }
    return s_kid;
}

void wifi_sync_set_kid(const char *kid)
{
    if (!kid || (strcmp(kid, "meixi") != 0 && strcmp(kid, "zhuangzhuang") != 0)) {
        return;
    }
    strncpy(s_kid, kid, sizeof(s_kid) - 1);
    s_kid[sizeof(s_kid) - 1] = '\0';
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, NVS_KEY_KID, s_kid);
        nvs_commit(h);
        nvs_close(h);
    }
}

void wifi_sync_toggle_kid(void)
{
    wifi_sync_set_kid(strcmp(wifi_sync_get_kid(), "meixi") == 0 ? "zhuangzhuang" : "meixi");
}

static char *s_fresh_json = NULL;   /* freshest good payload (owned) */
static uint32_t s_courses_version = 0;
static volatile bool s_syncing = false;

bool wifi_sync_is_syncing(void)
{
    return s_syncing;
}

static void update_fresh_json(char *json, bool save_cache)
{
    if (s_fresh_json) {
        free(s_fresh_json);
    }
    s_fresh_json = json;
    s_courses_version++;
    if (save_cache && json) {
        cache_save(json);
    }
}

esp_err_t wifi_sync_refresh_courses(void)
{
    char url[192];
    s_syncing = true;

    /* 1) LAN first */
    snprintf(url, sizeof(url), "%s/api/today?kid=%s", CONFIG_API_LAN_URL, wifi_sync_get_kid());
    char *json = http_get(url);

    /* 2) Cloudflare fallback */
    if (!json && strlen(CONFIG_API_CLOUD_URL) > 0) {
        ESP_LOGI(TAG, "LAN fetch failed, trying cloud URL");
        snprintf(url, sizeof(url), "%s/api/today?kid=%s", CONFIG_API_CLOUD_URL, wifi_sync_get_kid());
        json = http_get(url);
    }

    if (!json) {
        s_syncing = false;
        return ESP_FAIL;
    }
    int probe_max = 4;
    course_t probe[4];
    int n = parse_courses_json(json, probe, probe_max);
    if (n < 0) {
        ESP_LOGE(TAG, "invalid JSON from server");
        free(json);
        s_syncing = false;
        return ESP_FAIL;
    }
    update_fresh_json(json, true);
    s_syncing = false;
    return ESP_OK;
}

int wifi_sync_get_courses(course_t *out, int max)
{
    /* Lazy-load the NVS cache once, so the UI has data before first fetch */
    if (!s_fresh_json) {
        char *cached = cache_load();
        if (cached) {
            update_fresh_json(cached, false);
            s_courses_version = 0;  /* cached data is not "new" */
        }
    }
    if (!s_fresh_json) {
        return -1;
    }
    return parse_courses_json(s_fresh_json, out, max);
}

uint32_t wifi_sync_courses_version(void)
{
    return s_courses_version;
}

static void course_sync_task(void *arg)
{
    for (;;) {
        xEventGroupWaitBits(s_wifi_events, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
        esp_err_t err = wifi_sync_refresh_courses();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "course sync failed, using cached data");
            vTaskDelay(pdMS_TO_TICKS(60000));  /* retry sooner on failure */
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(COURSE_SYNC_INTERVAL_MS));
    }
}

esp_err_t wifi_sync_start_time_task(void)
{
    if (xTaskCreatePinnedToCore(time_sync_task, "ntp_sync", 4096, NULL, 3, NULL, 0) != pdPASS) {
        return ESP_FAIL;
    }
    return xTaskCreatePinnedToCore(course_sync_task, "course_sync", 8192, NULL, 3, NULL, 0) == pdPASS
           ? ESP_OK : ESP_FAIL;
}
