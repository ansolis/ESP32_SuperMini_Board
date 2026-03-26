// This header is for weather information retrieval using OpenWeather API

#ifndef WEATHER_H
#define WEATHER_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t fetch_seattle_weather(void);

#ifdef __cplusplus
}
#endif

#endif // WEATHER_H