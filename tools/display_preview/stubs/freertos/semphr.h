#pragma once
#include "FreeRTOS.h"
typedef void* SemaphoreHandle_t;
inline SemaphoreHandle_t xSemaphoreCreateMutex() { return (void*)1; }
inline BaseType_t xSemaphoreTake(SemaphoreHandle_t, TickType_t) { return 1; }
inline BaseType_t xSemaphoreGive(SemaphoreHandle_t) { return 1; }
