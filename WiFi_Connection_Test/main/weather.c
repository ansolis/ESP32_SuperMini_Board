// This module is for weather information retrieval using OpenWeather API

#include "weather.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "esp_log.h"
#include "string.h"
#include <stdint.h>
#include <netdb.h>
#include "esp_rom_sys.h"
#include "esp_netif.h"
#include "esp_crt_bundle.h"

static const char *TAG = "weather";

#define OPENWEATHER_API_URL "https://api.openweathermap.org"
#define SEATTLE_CITY "Seattle,US"

// Structure to hold parsed weather data
typedef struct {
    char city[50];
    char country[3];
    float temperature;
    char description[50];
} weather_data_t;

// Prints all fields of the JSON object as key:value pairs recursively
static void print_json(const cJSON *item, int depth, long timezone_offset)
{
    char indent[64] = {0};
    for (int i = 0; i < depth * 2 && i < (int)sizeof(indent) - 1; i++)
        indent[i] = ' ';

    cJSON *child = item->child;
    while (child) {
        const char *key = child->string ? child->string : "[array element]";

        if (cJSON_IsObject(child)) {
            ESP_LOGI(TAG, "%s%s:", indent, key);
            print_json(child, depth + 1, timezone_offset);
        } else if (cJSON_IsArray(child)) {
            ESP_LOGI(TAG, "%s%s:", indent, key);
            print_json(child, depth + 1, timezone_offset);
        } else if (cJSON_IsString(child)) {
            ESP_LOGI(TAG, "%s%s: %s", indent, key, child->valuestring);
        } else if (cJSON_IsNumber(child)) {
            if (child->valuedouble == (long long)child->valuedouble) {
                // Check if this field is a timestamp
                if (strcmp(key, "dt") == 0 || strcmp(key, "sunrise") == 0 || strcmp(key, "sunset") == 0) {
                    time_t utc_time = (time_t)child->valuedouble;
                    time_t local_time = utc_time + timezone_offset;
                    struct tm *tm_info = gmtime(&local_time);
                    char buf[32];
                    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm_info);
                    ESP_LOGI(TAG, "%s%s: %s", indent, key, buf);
                } else {
                    ESP_LOGI(TAG, "%s%s: %lld", indent, key, (long long)child->valuedouble);
                }
            } else {
                ESP_LOGI(TAG, "%s%s: %.2f", indent, key, child->valuedouble);
            }
        } else if (cJSON_IsBool(child)) {
            ESP_LOGI(TAG, "%s%s: %s", indent, key, cJSON_IsTrue(child) ? "true" : "false");
        } else if (cJSON_IsNull(child)) {
            ESP_LOGI(TAG, "%s%s: null", indent, key);
        }
        child = child->next;
    }
}

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


        // Extract timezone to ensure all time fields are printed
        // for the current time zone for the given locale
        cJSON *timezone = cJSON_GetObjectItemCaseSensitive(root, "timezone");
        long timezone_offset = 0;
        if (timezone && cJSON_IsNumber(timezone)) {
            timezone_offset = (long)timezone->valuedouble;
        }
        ESP_LOGD(TAG, "Timezone offset: %ld seconds", timezone_offset);
        print_json(root, 0, timezone_offset);
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
    // Units can be "metric", "imperial" or "standard"
    snprintf(request_url, sizeof(request_url),
        "%s/data/2.5/weather?q=%s&appid=%s&units=imperial",
        OPENWEATHER_API_URL, city, OPENWEATHER_API_KEY);

    esp_http_client_config_t config = {
        .url = request_url,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,  // use built-in cert bundle
    };

    ESP_LOGD(TAG, "Free heap: %lu, Min free heap: %lu",
        esp_get_free_heap_size(),
        esp_get_minimum_free_heap_size());

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "HTTP client initialized successfully for TLS/SSL");

    ESP_LOGI(TAG, "Attempting to open TLS/SSL connection...");
    esp_http_client_set_header(client, "Accept-Encoding", "identity");
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open TLS/SSL connection: %s (0x%04X)", esp_err_to_name(err), err);

        // Additional debugging for common issues
        if (err == ESP_FAIL) {
            ESP_LOGE(TAG, "General TLS handshake failure - possible causes:");
            ESP_LOGE(TAG, "1. Certificate doesn't match the server");
            ESP_LOGE(TAG, "2. Server name doesn't match certificate");
            ESP_LOGE(TAG, "3. TLS version mismatch");
            ESP_LOGE(TAG, "4. Network connectivity issues");
        }

        esp_http_client_cleanup(client);
        return err;
    }
    ESP_LOGI(TAG, "TLS/SSL connection established successfully");
    ESP_LOGI(TAG, "Fetching headers from HTTPS server...");
    int64_t content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "HTTPS Response - Status: %d, Content-Length: %lld", status_code, content_length);

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
    // Check if a DNS server is available
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_dns_info_t dns;
    esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns);
    ESP_LOGD(TAG, "DNS server: " IPSTR, IP2STR(&dns.ip.u_addr.ip4));

    return fetch_weather(SEATTLE_CITY);
}
