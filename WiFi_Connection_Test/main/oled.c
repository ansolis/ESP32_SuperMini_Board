/*
 * OLED Display Driver Implementation for SSD1315 (compatible with SSD1305)
 *
 * This file implements the OLED display driver for a 0.96" 128x64 OLED display
 * using the I2C interface on ESP32-S3.
 */

#include "oled.h"
#include "oled_private.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Tag for logging
static const char *TAG = "OLED";

// Framebuffer for the OLED display (128x64 pixels, 1 bit per pixel)
static uint8_t oled_framebuffer[OLED_WIDTH * OLED_HEIGHT / 8] = {0};

// Initialize I2C bus
static esp_err_t i2c_master_init() {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    esp_err_t ret = i2c_param_config(I2C_MASTER_PORT, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure I2C parameters: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2c_driver_install(I2C_MASTER_PORT, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install I2C driver: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

// Send a command to the OLED display
static esp_err_t oled_send_command(uint8_t command) {
    uint8_t buffer[2] = {0x00, command}; // 0x00 indicates command

    esp_err_t ret = i2c_master_write_to_device(I2C_MASTER_PORT, OLED_I2C_ADDRESS,
                                               buffer, sizeof(buffer), pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send command 0x%02X: %s", command, esp_err_to_name(ret));
    }

    return ret;
}

// Send data to the OLED display
static esp_err_t oled_send_data(uint8_t *data, size_t length) {
    uint8_t *buffer = malloc(length + 1);
    if (buffer == NULL) {
        return ESP_ERR_NO_MEM;
    }

    buffer[0] = 0x40; // 0x40 indicates data
    memcpy(buffer + 1, data, length);

    esp_err_t ret = i2c_master_write_to_device(I2C_MASTER_PORT, OLED_I2C_ADDRESS,
                                               buffer, length + 1, pdMS_TO_TICKS(100));
    free(buffer);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send data: %s", esp_err_to_name(ret));
    }

    return ret;
}

// Initialize the OLED display
esp_err_t oled_init(void) {
    ESP_LOGI(TAG, "Initializing OLED display...");

    // Initialize I2C
    esp_err_t ret = i2c_master_init();
    if (ret != ESP_OK) {
        return ret;
    }

    // Wait for display to be ready
    vTaskDelay(pdMS_TO_TICKS(100));

    // Send initialization commands
    oled_send_command(OLED_CMD_DISPLAY_OFF);                    // Display off
    oled_send_command(OLED_CMD_SET_DISPLAY_CLOCK_DIV);         // Set display clock divide ratio
    oled_send_command(0x80);                                  // Default ratio

    oled_send_command(OLED_CMD_SET_MULTIPLEX);                 // Set multiplex ratio
    oled_send_command(0x3F);                                  // 64 COM lines

    oled_send_command(OLED_CMD_SET_DISPLAY_OFFSET);             // Set display offset
    oled_send_command(0x00);                                  // No offset

    oled_send_command(OLED_CMD_SET_START_LINE | 0x00);         // Set start line at line 0

    oled_send_command(OLED_CMD_CHARGE_PUMP);                   // Charge pump
    oled_send_command(OLED_CMD_SWITCH_CAP_VCC);                // Use internal DC-DC

    oled_send_command(OLED_CMD_MEMORY_MODE);                   // Set memory addressing mode
    oled_send_command(0x00);                                  // Horizontal addressing mode

    oled_send_command(OLED_CMD_SEG_REMAP | 0x01);             // Set segment re-map (column address 127 is mapped to SEG0)

    oled_send_command(OLED_CMD_COM_SCAN_DEC);                  // Set COM output scan direction

    oled_send_command(OLED_CMD_SET_COMPINS);                   // Set COM pins hardware configuration
    oled_send_command(0x12);                                  // Alternative COM pin configuration

    oled_send_command(OLED_CMD_SET_CONTRAST);                  // Set contrast control
    oled_send_command(0xCF);                                  // Default contrast

    oled_send_command(OLED_CMD_SET_PRECHARGE);                 // Set pre-charge period
    oled_send_command(0xF1);                                  // Phase 1: 15 clocks, Phase 2: 1 clock

    oled_send_command(OLED_CMD_SET_VCOM_DETECT);               // Set VCOMH deselect level
    oled_send_command(0x40);                                  // ~0.77xVcc

    oled_send_command(OLED_CMD_DISPLAY_ALL_ON_RESUME);         // Disable entire display on

    oled_send_command(OLED_CMD_NORMAL_DISPLAY);                // Set normal display (not inverted)

    oled_send_command(OLED_CMD_DISPLAY_ON);                    // Display on

    // Clear the display
    oled_clear();

    ESP_LOGI(TAG, "OLED display initialized successfully");
    return ESP_OK;
}

// Clear the OLED display
esp_err_t oled_clear(void) {
    memset(oled_framebuffer, 0, sizeof(oled_framebuffer));
    return oled_update_display();
}

// Draw a pixel at the specified coordinates
esp_err_t oled_draw_pixel(uint8_t x, uint8_t y, uint8_t color) {
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) {
        return ESP_ERR_INVALID_ARG;
    }

    // Calculate byte and bit position in the framebuffer
    uint16_t byte_index = x + (y / 8) * OLED_WIDTH;
    uint8_t bit_mask = 1 << (y % 8);

    if (color) {
        oled_framebuffer[byte_index] |= bit_mask;  // Set pixel (white)
    } else {
        oled_framebuffer[byte_index] &= ~bit_mask; // Clear pixel (black)
    }

    return ESP_OK;
}

// Draw a filled rectangle
esp_err_t oled_draw_rectangle(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t color) {
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) {
        return ESP_ERR_INVALID_ARG;
    }

    // Ensure the rectangle stays within display bounds
    uint8_t end_x = x + width;
    uint8_t end_y = y + height;
    if (end_x > OLED_WIDTH) end_x = OLED_WIDTH;
    if (end_y > OLED_HEIGHT) end_y = OLED_HEIGHT;

    for (uint8_t current_y = y; current_y < end_y; current_y++) {
        for (uint8_t current_x = x; current_x < end_x; current_x++) {
            oled_draw_pixel(current_x, current_y, color);
        }
    }

    return ESP_OK;
}

// Update the OLED display with the current framebuffer
esp_err_t oled_update_display(void) {
    // Set column address range (0 to 127)
    oled_send_command(OLED_CMD_SET_LOW_COLUMN | 0x00);
    oled_send_command(OLED_CMD_SET_HIGH_COLUMN | 0x00);
    oled_send_command(OLED_CMD_SET_START_LINE | 0x00);

    // Set page address range (0 to 7 for 64 pixels / 8 pixels per page)
    oled_send_command(0xB0); // Page start address

    // Send the entire framebuffer
    return oled_send_data(oled_framebuffer, sizeof(oled_framebuffer));
}
