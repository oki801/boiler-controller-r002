#pragma once
#include <Arduino.h>
#include <mbedtls/sha256.h>

// out: legalább 65 bájt (64 hex + NUL)
inline void sha256Hex(const char* in, char* out) {
  uint8_t h[32];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  mbedtls_sha256_update(&ctx, reinterpret_cast<const unsigned char*>(in), strlen(in));
  mbedtls_sha256_finish(&ctx, h);
  mbedtls_sha256_free(&ctx);
  static const char* hx = "0123456789abcdef";
  for (int i = 0; i < 32; ++i) { out[2 * i] = hx[h[i] >> 4]; out[2 * i + 1] = hx[h[i] & 15]; }
  out[64] = 0;
}
