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

#ifdef __cplusplus
extern "C" {
#endif
#include "lodepng.h"
#ifdef __cplusplus
}
#endif

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

static void print_icon_ascii(const uint8_t *pixels, unsigned width, unsigned height)
{
    char line[26] = {0};  // 25 chars + null terminator

    for (unsigned y = 0; y < height; y += 2) {       // skip every other row
        int col = 0;
        for (unsigned x = 0; x < width; x += 2) {   // skip every other column
            unsigned idx = (y * width + x) * 4;      // RGBA = 4 bytes per pixel
            uint8_t r = pixels[idx];
            uint8_t g = pixels[idx + 1];
            uint8_t b = pixels[idx + 2];
            uint8_t a = pixels[idx + 3];

            // Standard luminance formula
            uint8_t luminance = (r * 299 + g * 587 + b * 114) / 1000;

            // Transparent or dark = space, light = asterisk
            // line[col++] = (a < 128 || luminance < 128) ? ' ' : '*';
            // Use 3 density levels:
            //  - transparent = space,
            //  - bright = "*"
            //  - mid-tone = "."
            line[col++] = (a < 128) ? ' ' : (luminance > 200) ? '*' : '.';
        }
        line[col] = '\0';
        ESP_LOGI(TAG, "|%s|", line);  // pipes make whitespace-only rows visible
    }
}

static esp_err_t download_weather_icon(const char *icon_code)
{
    char icon_url[128];
    snprintf(icon_url, sizeof(icon_url),
        // "https://openweathermap.org/img/wn/%s.png", icon_code);
        "http://openweathermap.org/img/wn/%s.png", icon_code);
    ESP_LOGI(TAG, "Downloading icon from: %s", icon_url);

    esp_http_client_config_t config = {
        .url = icon_url,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client for icon");
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open icon connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    int64_t content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "Icon response - Status: %d, Size: %lld bytes", status_code, content_length);

    if (status_code != 200 || content_length <= 0) {
        ESP_LOGE(TAG, "Unexpected icon response, status: %d", status_code);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    // Download raw PNG bytes
    uint8_t *png_data = malloc(content_length);
    if (!png_data) {
        ESP_LOGE(TAG, "Failed to allocate %lld bytes for PNG", content_length);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }

    int read_len = esp_http_client_read(client, (char *)png_data, content_length);
    ESP_LOGI(TAG, "Read %d bytes of PNG data", read_len);

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    // Decode PNG to RGBA pixels
    uint8_t *pixels = NULL;
    unsigned width, height;
    unsigned decode_err = lodepng_decode32(&pixels, &width, &height, png_data, read_len);
    free(png_data);  // done with raw PNG bytes

    if (decode_err) {
        ESP_LOGE(TAG, "PNG decode failed: %s", lodepng_error_text(decode_err));
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Icon decoded: %ux%u px, rendering as %ux%u ASCII art",
        width, height, width / 2, height / 2);

    print_icon_ascii(pixels, width, height);
    free(pixels);

    return ESP_OK;
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

        // Download and print the weather icon to console (for now)
        // cJSON *weather_array = cJSON_GetObjectItemCaseSensitive(root, "weather");
        // cJSON *weather_obj = cJSON_GetArrayItem(weather_array, 0);
        cJSON *icon_obj = cJSON_GetObjectItemCaseSensitive(weather_obj, "icon");
        if (icon_obj && cJSON_IsString(icon_obj)) {
            ESP_LOGI(TAG, "Calling download_weather_icon()");
            download_weather_icon(icon_obj->valuestring);
        } else {
            ESP_LOGE(TAG, "download_weather_icon() was not called");
            if (icon_obj) {
                ESP_LOGI(TAG, "icon_obj is not NULL");
            } else {
                ESP_LOGE(TAG, "icon_obj is NULL");
            }
            if (cJSON_IsString(icon_obj)) {
                ESP_LOGI(TAG, "cJSON_IsString(icon_obj) is true");
            } else {
                ESP_LOGE(TAG, "cJSON_IsString(icon_obj) is NULL");
            }
        }
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

        esp_http_client_close(client);
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
        if (response == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for response");
            esp_http_client_close(client);
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
