#pragma once
#include <stdint.h>

struct RtttlNote { uint16_t freq_hz; uint16_t dur_ms; };  // freq 0 = pausa

class Rtttl {
 public:
  bool begin(const char* tune);     // parseia header "name:d=4,o=5,b=180:"
  bool next(RtttlNote& out);        // false = acabou
 private:
  const char* p_ = nullptr;
  uint8_t def_dur_ = 4, def_oct_ = 5;
  uint16_t bpm_ = 63;
};
