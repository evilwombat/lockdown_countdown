#include <stdio.h>
#include <stdarg.h>
#include "util.h"
#include "main.h"

extern UART_HandleTypeDef huart1;
extern RTC_HandleTypeDef hrtc;

void uart_printf(const char *fmt, ...)
{
    char buffer[256];
    int len;
    va_list args;
    va_start(args, fmt);
    len = vsnprintf(buffer, 256, fmt, args);
    va_end(args);

    HAL_UART_Transmit(&huart1, (unsigned char *) buffer, len, -1);
}

int get_time_seconds()
{
    static int last_sec = -1;

    RTC_TimeTypeDef rtc_time;

    HAL_RTC_GetTime(&hrtc, &rtc_time, RTC_FORMAT_BIN);

    if (rtc_time.Seconds != last_sec) {

        uart_printf("%02d:%02d:%02d\n", rtc_time.Hours, rtc_time.Minutes, rtc_time.Seconds);
        last_sec = rtc_time.Seconds;
    }

    return (rtc_time.Hours * 60 + rtc_time.Minutes) * 60 + rtc_time.Seconds;
}

int get_time_minutes()
{
    return get_time_seconds() / 60;
}
