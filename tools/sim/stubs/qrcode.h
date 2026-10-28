// Stand-in for ricmoo/QRCode backed by matrices pre-generated in Python.
#pragma once
#include "Arduino.h"
#include "qr_table.h"
#define ECC_LOW 0
struct QRCode { uint8_t version; uint8_t size; const char* bits; };
inline uint16_t qrcode_getBufferSize(uint8_t v) { int s = 4 * v + 17; return (s * s + 7) / 8; }
inline int8_t qrcode_initText(QRCode* q, uint8_t*, uint8_t version, uint8_t, const char* text) {
  for (auto& e : SIM_QR) {
    if (strcmp(e.t, text) == 0) {
      if (e.size != 4 * version + 17) { fprintf(stderr, "QR version mismatch\n"); return -1; }
      q->version = version; q->size = e.size; q->bits = e.bits; return 0;
    }
  }
  fprintf(stderr, "QR text not in sim table: %s\n", text);
  return -1;
}
inline bool qrcode_getModule(QRCode* q, uint8_t x, uint8_t y) { return q->bits[y * q->size + x] == '1'; }
