// This module is for current time retrieval over SNTP protocol

#include "esp_sntp.h"
#include "esp_netif_sntp.h"
#include <time.h>
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "sntp";
static esp_timer_handle_t time_log_timer;

void print_current_time(void)
{
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
        ESP_LOGI(TAG, "Time not set yet");
    }
}

void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&config);

    // Create a periodic timer to log time every 10 seconds
    const esp_timer_create_args_t timer_args = {
        .callback = (esp_timer_cb_t)print_current_time,
        .name = "time_log_timer"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &time_log_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(time_log_timer, 10 * 1000000)); // 10 seconds
}
