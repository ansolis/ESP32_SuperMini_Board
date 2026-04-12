// WS2812 RGB LED driver header file

#pragma once

#include "led_strip.h"

// Define WS2812B RGB LED configuration
#define RGB_LED_GPIO        GPIO_NUM_48  // Data pin for WS2812B LED
#define RGB_LED_NUM_PIXELS 1            // Number of LEDs in the strip

// Color definitions
typedef enum {
    RED,
    GREEN,
    BLUE,
    YELLOW,
    CYAN,
    MAGENTA,
    WHITE,
    BLACK
} rgb_color_t;

/**
 * @brief Initialize the WS2812B RGB LED driver
 */
void rgb_led_init(void);

/**
 * @brief Set the WS2812B RGB LED color
 *
 * @param color The color to set
 */
void rgb_led_set_color(rgb_color_t color);
