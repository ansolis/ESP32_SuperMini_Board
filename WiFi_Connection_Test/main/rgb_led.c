// RGB LED strip driver
// Wraps the LED strip driver to provide simple color control for a single WS2812 LED

#include "rgb_led.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MAX_BRIGHTNESS 5  // Goes up to 255

static const char *TAG = "RGB_LED";

// LED strip handle
static led_strip_handle_t led_strip;

void rgb_led_init(void)
{
    ESP_LOGI(TAG, "Initializing WS2812B RGB LED driver on GPIO %d", RGB_LED_GPIO);

    // LED strip configuration
    led_strip_config_t strip_config = {
        .strip_gpio_num = RGB_LED_GPIO,
        .max_leds = RGB_LED_NUM_PIXELS, // Single LED on board
    };

    // RMT configuration for LED strip
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10 MHz
        .flags.with_dma = false,
    };

    // Install LED strip driver
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

    // Clear the LED (turn it off)
    led_strip_clear(led_strip);

    ESP_LOGI(TAG, "WS2812B RGB LED driver initialized successfully");
}

void rgb_led_set_color(rgb_color_t color)
{
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;

    // Convert color enum to RGB values
    switch (color) {
        case RED:
            red = MAX_BRIGHTNESS;
            break;
        case GREEN:
            green = MAX_BRIGHTNESS;
            break;
        case BLUE:
            blue = MAX_BRIGHTNESS;
            break;
        case YELLOW:
            red = MAX_BRIGHTNESS;
            green = MAX_BRIGHTNESS;
            break;
        case CYAN:
            green = MAX_BRIGHTNESS;
            blue = MAX_BRIGHTNESS;
            break;
        case MAGENTA:
            red = MAX_BRIGHTNESS;
            blue = MAX_BRIGHTNESS;
            break;
        case WHITE:
            red = MAX_BRIGHTNESS;
            green = MAX_BRIGHTNESS;
            blue = MAX_BRIGHTNESS;
            break;
        case BLACK:
        default:
            // LED off - just clear and return
            led_strip_clear(led_strip);
            led_strip_refresh(led_strip);
            ESP_LOGI(TAG, "Setting WS2812B color: R=0, G=0, B=0");
            return;
    }

    ESP_LOGI(TAG, "Setting WS2812B color: R=%d, G=%d, B=%d", red, green, blue);

    // Set the LED pixel using RGB values (0-255)
    led_strip_set_pixel(led_strip, 0, red, green, blue);

    // Refresh the strip to send data
    led_strip_refresh(led_strip);
}
