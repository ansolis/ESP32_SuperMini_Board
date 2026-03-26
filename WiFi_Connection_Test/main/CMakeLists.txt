idf_component_register(SRCS "fast_scan.c" "sntp_time.c" "weather.c"
                    INCLUDE_DIRS ".")

if(DEFINED ENV{OPENWEATHER_API_KEY})
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        OPENWEATHER_API_KEY="$ENV{OPENWEATHER_API_KEY}"
    )
else()
    message(FATAL_ERROR "OPENWEATHER_API_KEY environment variable is not set")
endif()
