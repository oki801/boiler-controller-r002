#pragma once
// PC-s csonk az Arduino maghoz: csak annyi, hogy a Display.cpp és az Adafruit GFX lefordüljon és lefusson.
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <algorithm>
#include "WString.h"
#include "Print.h"
#define PROGMEM
#define pgm_read_byte(a)  (*(const uint8_t*)(a))
#define pgm_read_word(a)  (*(const uint16_t*)(a))
#define pgm_read_dword(a) (*(const uint32_t*)(a))
#define pgm_read_pointer(a) ((void*)(*(const uintptr_t*)(a)))
using std::min; using std::max;
inline uint32_t millis() { extern uint32_t g_millis; return g_millis; }
struct SerialStub { template <typename... A> void printf(const char*, A...) {} };
extern SerialStub Serial;
inline size_t strlcpy_(char* d, const char* s, size_t n) { size_t l = strlen(s); if (n) { size_t c = l >= n ? n - 1 : l; memcpy(d, s, c); d[c] = 0; } return l; }
#ifndef strlcpy
#define strlcpy strlcpy_
#endif
#define INPUT_PULLUP 2
#define OUTPUT 1
#define LOW 0
#define HIGH 1
inline void pinMode(int, int) {}
inline int digitalRead(int) { return 1; }
inline void digitalWrite(int, int) {}
#define radians(deg) ((deg) * 0.017453292519943295)
