#pragma once
#include <stdint.h>
typedef uint32_t TickType_t; typedef unsigned UBaseType_t; typedef uint8_t StackType_t; typedef int BaseType_t;
#define pdMS_TO_TICKS(x) (x)
#define portMAX_DELAY 0xFFFFFFFFu
#define pdTRUE 1
inline void vTaskDelay(TickType_t) {}
inline UBaseType_t uxTaskGetStackHighWaterMark(void*) { return 4000; }
inline BaseType_t xTaskCreatePinnedToCore(void (*)(void*), const char*, uint32_t, void*, UBaseType_t, void*, BaseType_t) { return 1; }
