// This module is for weather information retrieval using OpenWeather API

#include "weather.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "esp_log.h"
#include "string.h"
static const char *TAG = "weather";

#define OPENWEATHER_API_URL "http://api.openweathermap.org"
#define SEATTLE_CITY "Seattle,US"

// Structure to hold parsed weather data
typedef struct {
    char city[50];
    char country[3];
    float temperature;
    char description[50];
} weather_data_t;

// Function to parse JSON response
static esp_err_t parse_weather_data(const char *json_response, weather_data_t *weather)
{
    cJSON *root = cJSON_Parse(json_response);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr) {
            ESP_LOGE(TAG, "JSON Error: %s", error_ptr);
        }
        return ESP_FAIL;
    }

    cJSON *main = cJSON_GetObjectItemCaseSensitive(root, "main");
    cJSON *weather_array = cJSON_GetObjectItemCaseSensitive(root, "weather");
    cJSON *sys = cJSON_GetObjectItemCaseSensitive(root, "sys");
    cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "name");

    if (main && weather_array && cJSON_GetArraySize(weather_array) > 0 && sys && name) {
        // Extract temperature
        cJSON *temp_obj = cJSON_GetObjectItemCaseSensitive(main, "temp");
        if (temp_obj && cJSON_IsNumber(temp_obj)) {
            weather->temperature = (float)temp_obj->valuedouble;
        }

        // Extract weather description
        cJSON *weather_obj = cJSON_GetArrayItem(weather_array, 0);
        cJSON *desc_obj = cJSON_GetObjectItemCaseSensitive(weather_obj, "description");
        if (desc_obj && cJSON_IsString(desc_obj)) {
            strncpy(weather->description, desc_obj->valuestring, sizeof(weather->description) - 1);
            weather->description[sizeof(weather->description) - 1] = '\0';
        }

        // Extract country
        cJSON *country_obj = cJSON_GetObjectItemCaseSensitive(sys, "country");
        if (country_obj && cJSON_IsString(country_obj)) {
            strncpy(weather->country, country_obj->valuestring, sizeof(weather->country) - 1);
            weather->country[sizeof(weather->country) - 1] = '\0';
        }

        // Extract city name
        if (cJSON_IsString(name)) {
            strncpy(weather->city, name->valuestring, sizeof(weather->city) - 1);
            weather->city[sizeof(weather->city) - 1] = '\0';
        }
        ESP_LOGI(TAG, "Weather in %s, %s: %.1f°C, %s",
                weather->city, weather->country, weather->temperature, weather->description);
    } else {
        ESP_LOGE(TAG, "Failed to parse weather data from JSON");
    cJSON_Delete(root);
        return ESP_FAIL;
    }

    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t fetch_weather(const char *city)
{
    ESP_LOGI(TAG, "Fetching weather for %s", city);

    static char request_url[256];
    snprintf(request_url, sizeof(request_url),
        "%s/data/2.5/weather?q=%s&appid=%s&units=metric",
        OPENWEATHER_API_URL, city, OPENWEATHER_API_KEY);

    ESP_LOGI(TAG, "API Request URL: %s", request_url);

    esp_http_client_config_t config = {
        .url = request_url,
        .transport_type = HTTP_TRANSPORT_OVER_TCP,
        .timeout_ms = 10000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
}

    esp_http_client_set_header(client, "Accept-Encoding", "identity");

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    int64_t content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "Status: %d, Content-Length: %lld", status_code, content_length);

    if (status_code == 200 && content_length > 0) {
        char *response = malloc(content_length + 1);
        if (!response) {
            ESP_LOGE(TAG, "Failed to allocate memory for response");
            esp_http_client_cleanup(client);
            return ESP_ERR_NO_MEM;
        }

        int read_len = esp_http_client_read(client, response, content_length);
        response[read_len] = '\0';
        ESP_LOGI(TAG, "Raw API Response: %s", response);

        // Parse the JSON response using the separate function
        weather_data_t weather;
        err = parse_weather_data(response, &weather);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to parse weather data");
        }

        free(response);
    } else {
        ESP_LOGE(TAG, "Weather API request failed with status code: %d", status_code);
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return err;
}

esp_err_t fetch_seattle_weather(void)
{
    return fetch_weather(SEATTLE_CITY);
}
