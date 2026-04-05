/*
 * OLED Display Driver for SSD1315 (compatible with SSD1305)
 *
 * This header file defines the interface for controlling a 0.96" 128x64 OLED display
 * using the I2C interface on ESP32-S3.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// OLED display dimensions
#define OLED_WIDTH 128
#define OLED_HEIGHT 64

// I2C address for SSD1315 OLED (0x3C or 0x3D depending on SA0 pin)
#define OLED_I2C_ADDRESS 0x3C

// Function prototypes
/**
 * @brief Initialize the OLED display
 *
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t oled_init(void);

/**
 * @brief Clear the OLED display
 *
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t oled_clear(void);

/**
 * @brief Draw a pixel at the specified coordinates
 *
 * @param x X coordinate (0-127)
 * @param y Y coordinate (0-63)
 * @param color 1 for white, 0 for black
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t oled_draw_pixel(uint8_t x, uint8_t y, uint8_t color);

/**
 * @brief Draw a filled rectangle
 *
 * @param x Start X coordinate
 * @param y Start Y coordinate
 * @param width Width of rectangle
 * @param height Height of rectangle
 * @param color 1 for white, 0 for black
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t oled_draw_rectangle(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t color);

/**
 * @brief Update the OLED display with the current framebuffer
 *
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t oled_update_display(void);
