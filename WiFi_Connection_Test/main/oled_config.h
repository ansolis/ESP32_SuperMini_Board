/*
 * OLED display configuration file
 *
 * This header file contains the configuration parameters for the OLED display.
 */

#pragma once

// I2C address for SSD1315 OLED (0x3C or 0x3D depending on SA0 pin)
#define OLED_I2C_ADDRESS 0x3C

// I2C configuration (internal use only)
#define I2C_MASTER_SCL_IO 13       // GPIO pin for SCL
#define I2C_MASTER_SDA_IO 12       // GPIO pin for SDA
#define I2C_MASTER_FREQ_HZ 400000  // I2C clock frequency
#define I2C_MASTER_PORT 0          // I2C port number
