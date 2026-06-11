#include "rtc.h"
#include <Arduino.h>
#include <Wire.h>
#include "clockfmt.h"
#include "pins.h"

namespace rtc {
static const uint8_t kAddr = 0x68;
static bool ok = false;
static DateTime cache;
static uint32_t last_read = 0;

static uint8_t bcd2bin(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
static uint8_t bin2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

static bool read_regs(uint8_t reg, uint8_t* buf, uint8_t n) {
  Wire.beginTransmission(kAddr);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(kAddr, n) != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

static bool fetch(DateTime& dt) {
  uint8_t r[7];
  if (!read_regs(0x00, r, 7)) return false;
  dt.sec = bcd2bin(r[0] & 0x7F);
  dt.min = bcd2bin(r[1] & 0x7F);
  dt.hour = bcd2bin(r[2] & 0x3F);              // sempre operamos em modo 24h
  dt.dow = (uint8_t)((r[3] & 0x07) - 1) % 7;   // chip guarda 1..7 (1=domingo)
  dt.day = bcd2bin(r[4] & 0x3F);
  dt.month = bcd2bin(r[5] & 0x1F);
  dt.year = 2000 + bcd2bin(r[6]);
  return true;
}

static bool lost_power() {
  uint8_t st;
  return read_regs(0x0F, &st, 1) && (st & 0x80);  // OSF: oscilador parou
}

// hora de compilacao como semente quando a bateria zerou
static DateTime build_time() {
  static const char* kMonths = "JanFebMarAprMayJunJulAugSepOctNovDec";
  char mon[4] = {__DATE__[0], __DATE__[1], __DATE__[2], '\0'};
  DateTime dt;
  dt.month = (uint8_t)((strstr(kMonths, mon) - kMonths) / 3 + 1);
  dt.day = (uint8_t)atoi(__DATE__ + 4);
  dt.year = (uint16_t)atoi(__DATE__ + 7);
  dt.hour = (uint8_t)atoi(__TIME__);
  dt.min = (uint8_t)atoi(__TIME__ + 3);
  dt.sec = (uint8_t)atoi(__TIME__ + 6);
  dt.dow = date_weekday(dt.year, dt.month, dt.day);
  return dt;
}

bool init() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.beginTransmission(kAddr);
  ok = (Wire.endTransmission() == 0);
  if (!ok) return false;
  if (lost_power()) write(build_time());
  ok = fetch(cache);
  last_read = millis();
  return ok;
}

bool present() { return ok; }

bool now(DateTime& dt) {
  if (!ok) return false;
  uint32_t ms = millis();
  if (ms - last_read >= 1000) {
    if (fetch(cache)) last_read = ms;
  }
  dt = cache;
  return true;
}

bool write(const DateTime& dt) {
  Wire.beginTransmission(kAddr);
  Wire.write((uint8_t)0x00);
  Wire.write(bin2bcd(dt.sec));
  Wire.write(bin2bcd(dt.min));
  Wire.write(bin2bcd(dt.hour));            // bit6=0: modo 24h
  Wire.write((uint8_t)(dt.dow + 1));       // 1..7 (1=domingo)
  Wire.write(bin2bcd(dt.day));
  Wire.write(bin2bcd(dt.month));           // bit7 (seculo) = 0
  Wire.write(bin2bcd((uint8_t)(dt.year - 2000)));
  if (Wire.endTransmission() != 0) return false;
  uint8_t st;                              // limpa o OSF: hora voltou a valer
  if (read_regs(0x0F, &st, 1)) {
    Wire.beginTransmission(kAddr);
    Wire.write((uint8_t)0x0F);
    Wire.write((uint8_t)(st & 0x7F));
    Wire.endTransmission();
  }
  cache = dt;
  last_read = millis();
  return true;
}

float temperature() {
  uint8_t r[2];
  if (!read_regs(0x11, r, 2)) return 0.0f;
  int16_t t = (int16_t)((int8_t)r[0] << 2) | (r[1] >> 6);
  return t * 0.25f;
}
}  // namespace rtc
