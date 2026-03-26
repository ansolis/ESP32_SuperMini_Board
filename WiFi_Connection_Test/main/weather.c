// This module is for weather information retrieval using OpenWeather API

#include "weather.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "string.h"

static const char *TAG = "weather";

#define OPENWEATHER_API_URL "http://api.openweathermap.org"
#define SEATTLE_CITY "Seattle,US"

static char request_url[256];

static esp_err_t fetch_weather(const char *city)
{
    ESP_LOGI(TAG, "Fetching weather for %s", city);
    snprintf(request_url, sizeof(request_url),
        "%s/data/2.5/weather?q=%s&appid=%s&units=metric",
        OPENWEATHER_API_URL, city, OPENWEATHER_API_KEY);

    ESP_LOGD(TAG, "API Request URL: %s", request_url);

    esp_http_client_config_t config = {
        .url = request_url,
        .transport_type = HTTP_TRANSPORT_OVER_TCP,  // Use TCP instead of SSL
        .timeout_ms = 10000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }

    // Tell the server to send plain text, not gzip
    esp_http_client_set_header(client, "Accept-Encoding", "identity");

    // Open connection and send request, but don't read body yet
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    // Fetch headers — this returns the content length
    int64_t content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGD(TAG, "Status: %d, Content-Length: %lld", status_code, content_length);

    if (status_code == 200 && content_length > 0) {
        char *response = malloc(content_length + 1);
        if (!response) {
            esp_http_client_cleanup(client);
            return ESP_ERR_NO_MEM;
        }

        // NOW read the body — connection is still open
        int read_len = esp_http_client_read(client, response, content_length);
        response[read_len] = '\0';

        ESP_LOGI(TAG, "Raw API Response: %s", response);

        // TODO: JSON parsing logic here

        free(response);
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return err;
}

esp_err_t fetch_seattle_weather(void)
{
    return fetch_weather(SEATTLE_CITY);
    return 0;
}
