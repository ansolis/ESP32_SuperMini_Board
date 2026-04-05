/*
 * Private OLED Display Driver Definitions for SSD1315
 *
 * This header file contains internal definitions and macros that don't need
 * to be exposed to users of the OLED driver.
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"

// OLED command definitions (internal use only)
#define OLED_CMD_SET_CONTRAST 0x81
#define OLED_CMD_DISPLAY_ALL_ON_RESUME 0xA4
#define OLED_CMD_DISPLAY_ALL_ON 0xA5
#define OLED_CMD_NORMAL_DISPLAY 0xA6
#define OLED_CMD_INVERT_DISPLAY 0xA7
#define OLED_CMD_DISPLAY_OFF 0xAE
#define OLED_CMD_DISPLAY_ON 0xAF
#define OLED_CMD_SET_DISPLAY_OFFSET 0xD3
#define OLED_CMD_SET_COMPINS 0xDA
#define OLED_CMD_SET_VCOM_DETECT 0xDB
#define OLED_CMD_SET_DISPLAY_CLOCK_DIV 0xD5
#define OLED_CMD_SET_PRECHARGE 0xD9
#define OLED_CMD_SET_MULTIPLEX 0xA8
#define OLED_CMD_SET_LOW_COLUMN 0x00
#define OLED_CMD_SET_HIGH_COLUMN 0x10
#define OLED_CMD_SET_START_LINE 0x40
#define OLED_CMD_MEMORY_MODE 0x20
#define OLED_CMD_COM_SCAN_INC 0xC0
#define OLED_CMD_COM_SCAN_DEC 0xC8
#define OLED_CMD_SEG_REMAP 0xA0
#define OLED_CMD_CHARGE_PUMP 0x8D
#define OLED_CMD_EXTERNAL_VCC 0x1
#define OLED_CMD_SWITCH_CAP_VCC 0x2

// I2C configuration (internal use only)
#define I2C_MASTER_SCL_IO 10       // GPIO pin for SCL
#define I2C_MASTER_SDA_IO 11       // GPIO pin for SDA
#define I2C_MASTER_FREQ_HZ 400000  // I2C clock frequency
#define I2C_MASTER_PORT 0          // I2C port number
