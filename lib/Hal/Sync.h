#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class Mutex {
 public:
  Mutex() : h_(xSemaphoreCreateMutex()) {}
  void lock() const   { xSemaphoreTake(h_, portMAX_DELAY); }
  void unlock() const { xSemaphoreGive(h_); }
 private:
  SemaphoreHandle_t h_;
};

class Lock {
 public:
  explicit Lock(const Mutex& m) : m_(m) { m_.lock(); }
  ~Lock() { m_.unlock(); }
 private:
  const Mutex& m_;
};
