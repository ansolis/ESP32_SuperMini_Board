// This module is for current time retrieval over SNTP protocol

#include "esp_sntp.h"
#include "esp_netif_sntp.h"
#include <time.h>
#include "esp_timer.h"
#include "esp_log.h"
#include "sntp_time.h"

#define UPDATE_PERIOD_SEC 10  // Read time over SNTP and display it every 10 seconds
#define UPDATE_PERIOD_USEC ((UPDATE_PERIOD_SEC) * 1000000)  // Read time over SNTP and display it every 10 seconds

static const char *TAG = "sntp";
static esp_timer_handle_t time_log_timer;
static bool time_is_set = false;

static void sntp_sync_time_cb(struct timeval *tv);
static void print_current_time(void);


static void sntp_sync_time_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Time synchronized!");
    time_is_set = true;

    // Now you can safely display the current time
    print_current_time();
}

static void print_current_time(void)
{
    static uint32_t time_not_init_count = 0;
    if (!time_is_set) {
        ESP_LOGW(TAG, "Time not available: Waiting for time synchronization...");

        time_not_init_count++;
        if (time_not_init_count > 1) {
            initialize_sntp();
            time_not_init_count = 0;
        }
        return;
    }

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // Check if time is set (year > 2020)
    if (timeinfo.tm_year > (2020 - 1900)) {
        char strftime_buf[64];
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI(TAG, "Current time: %s", strftime_buf);
    } else {
        ESP_LOGW(TAG, "Time not set");
    }
}

void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");

    // Set the synchronization callback and smooth sync
    config.smooth_sync = true;
    config.sync_cb = sntp_sync_time_cb;

    esp_netif_sntp_init(&config);

    // Create a periodic timer to log time every 10 seconds
    const esp_timer_create_args_t timer_args = {
        .callback = (esp_timer_cb_t)print_current_time,
        .name = "time_log_timer"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &time_log_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(time_log_timer, UPDATE_PERIOD_USEC));
}
