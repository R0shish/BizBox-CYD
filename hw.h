#pragma once
// ==========================================================================
//  CYD hardware: pins, the XPT2046 touch controller (bit-banged) and the
//  backlight PWM. GPIO25 is the touch clock: audio code must never drive it.
// ==========================================================================

// ==========================================
// CYD Hardware Pin Definitions (ESP32-2432S028)
// ==========================================
#define CYD_T_MOSI 32
#define CYD_T_MISO 39
#define CYD_T_CLK  25
#define CYD_T_CS   33
#define CYD_T_IRQ  36
#define CYD_BACKLIGHT 21

// ==========================================
// Robust CYD Touch Driver (XPT2046 Bit-Bang)
// ==========================================
class CYDTouch {
private:
  uint16_t transfer(uint8_t cmd) {
    digitalWrite(CYD_T_CS, LOW);
    delayMicroseconds(2);

    // 1. Send 8-bit command (MSB first)
    for (int i = 7; i >= 0; i--) {
      digitalWrite(CYD_T_MOSI, (cmd >> i) & 1);
      delayMicroseconds(1);
      digitalWrite(CYD_T_CLK, HIGH);
      delayMicroseconds(2);
      digitalWrite(CYD_T_CLK, LOW);
      delayMicroseconds(1);
    }

    // 2. 9th clock: XPT2046 conversion dummy / BUSY bit
    digitalWrite(CYD_T_CLK, HIGH);
    delayMicroseconds(2);
    digitalWrite(CYD_T_CLK, LOW);
    delayMicroseconds(1);

    // 3. Read 12-bit ADC result (MSB first)
    uint16_t val = 0;
    for (int i = 11; i >= 0; i--) {
      digitalWrite(CYD_T_CLK, HIGH);
      delayMicroseconds(2);
      if (digitalRead(CYD_T_MISO)) {
        val |= (1 << i);
      }
      digitalWrite(CYD_T_CLK, LOW);
      delayMicroseconds(1);
    }

    digitalWrite(CYD_T_CS, HIGH);
    delayMicroseconds(2);
    return val;
  }

public:
  void begin() {
    pinMode(CYD_T_CS,   OUTPUT);
    pinMode(CYD_T_CLK,  OUTPUT);
    pinMode(CYD_T_MOSI, OUTPUT);
    pinMode(CYD_T_MISO, INPUT);
    pinMode(CYD_T_IRQ,  INPUT);

    digitalWrite(CYD_T_CS, HIGH);
    digitalWrite(CYD_T_CLK, LOW);
    digitalWrite(CYD_T_MOSI, LOW);
  }

  bool getRaw(int &x, int &y, int &z) {
    // Read Pressure Z1 and Z2 (Commands 0xB0 and 0xC0)
    uint16_t z1 = transfer(0xB0);
    uint16_t z2 = transfer(0xC0);
    z = (int)z1 + 4095 - (int)z2;

    // Detect touch via pressure Z OR hardware IRQ pin
    bool isDown = (digitalRead(CYD_T_IRQ) == LOW) || (z > 350) || (z1 > 120 && z1 < 3900);
    if (!isDown) return false;

    // Read X (0xD0) and Y (0x90) with oversampling
    int x1 = transfer(0xD0);
    int y1 = transfer(0x90);
    int x2 = transfer(0xD0);
    int y2 = transfer(0x90);

    if (abs(x1 - x2) > 150 || abs(y1 - y2) > 150) {
      x = x2;
      y = y2;
    } else {
      x = (x1 + x2) / 2;
      y = (y1 + y2) / 2;
    }

    return (x > 150 && x < 3950 && y > 150 && y < 3950);
  }

  bool getScreenTouch(int &screenX, int &screenY, bool swap, bool invX, bool invY) {
    int rx, ry, rz;
    if (!getRaw(rx, ry, rz)) return false;

    int mx, my;
    if (swap) {
      mx = map(ry, 240, 3800, 0, 320);
      my = map(rx, 240, 3800, 0, 240);
    } else {
      mx = map(rx, 240, 3800, 0, 320);
      my = map(ry, 240, 3800, 0, 240);
    }

    if (invX) mx = 319 - mx;
    if (invY) my = 239 - my;

    screenX = constrain(mx, 0, 319);
    screenY = constrain(my, 0, 239);
    return true;
  }
};

// ---------- Display Backlight PWM Dimmer ----------
void setBacklight(int percent) {
  percent = constrain(percent, 10, 100);
  int duty = map(percent, 10, 100, 25, 255);
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  ledcWrite(CYD_BACKLIGHT, duty);
#else
  ledcWrite(0, duty);
#endif
}

void initBacklight(int percent) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  ledcAttach(CYD_BACKLIGHT, 5000, 8);
#else
  ledcSetup(0, 5000, 8);
  ledcAttachPin(CYD_BACKLIGHT, 0);
#endif
  setBacklight(percent);
}
