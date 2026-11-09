#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "WString.h"
class Print {
 public:
  virtual ~Print() {}
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t* b, size_t n) { size_t k = 0; while (n--) k += write(*b++); return k; }
  size_t write(const char* s) { return write((const uint8_t*)s, strlen(s)); }
  size_t print(const char* s) { return write(s); }
  size_t print(char c) { return write((uint8_t)c); }
  size_t print(const String& s) { return write(s.c_str()); }
  size_t println(const char* s) { size_t k = write(s); return k + write((uint8_t)'\n'); }
  size_t println() { return write((uint8_t)'\n'); }
};
