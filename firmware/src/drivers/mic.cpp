#include "mic.h"

#include <Arduino.h>
#include <driver/i2s.h>

#include "pins.h"

namespace mic {

static bool instalado_ = false;
static bool rodando_ = false;

bool init() {
  if (instalado_) return true;
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate = 16000;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  // estereo de diagnostico: captura os 2 slots e read() escolhe o que tem
  // sinal (o INMP441 cai num slot diferente conforme driver/versao do IDF)
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = 0;
  cfg.dma_buf_count = 12;    // 12x256 frames ~ 192 ms de folga pro loop
  cfg.dma_buf_len = 256;
  if (i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) != ESP_OK) return false;
  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_PIN_NO_CHANGE;
  pins.bck_io_num = PIN_MIC_SCK;
  pins.ws_io_num = PIN_MIC_WS;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = PIN_MIC_SD;
  if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) return false;
  i2s_stop(I2S_NUM_0);       // dormindo ate o primeiro start()
  instalado_ = true;
  return true;
}

void start() {
  if (!instalado_ && !init()) return;
  if (rodando_) return;
  i2s_zero_dma_buffer(I2S_NUM_0);
  i2s_start(I2S_NUM_0);
  rodando_ = true;
}

size_t read(int16_t* buf, size_t max) {
  if (!instalado_) return 0;
  static int32_t cru[512];  // pares A/B intercalados (estereo)
  size_t pedir = max < 256 ? max : 256;
  size_t lidos = 0;
  if (i2s_read(I2S_NUM_0, cru, pedir * 2 * sizeof(int32_t), &lidos, 0) !=
      ESP_OK)
    return 0;
  size_t n = lidos / (2 * sizeof(int32_t));  // frames estereo completos
  if (!n) return 0;
  // mede os dois slots e fica com o que tem sinal
  uint64_t soma_a = 0, soma_b = 0;
  for (size_t i = 0; i < n; i++) {
    int32_t a = cru[2 * i] >> 16;      // 16 bits de cima dos 24: ganho 1x
    int32_t b = cru[2 * i + 1] >> 16;  // (>>14 estourava na fala normal)
    soma_a += (uint32_t)abs(a);
    soma_b += (uint32_t)abs(b);
  }
  size_t slot = soma_b > soma_a ? 1 : 0;
  for (size_t i = 0; i < n; i++)
    buf[i] = (int16_t)(cru[2 * i + slot] >> 16);
  static uint32_t log_ms_ = 0;  // diagnostico: nivel medio de cada slot
  uint32_t agora = millis();
  if ((int32_t)(agora - log_ms_) >= 0) {
    log_ms_ = agora + 1000;
    Serial.printf("[mic] slots A=%u B=%u\n", (unsigned)(soma_a / n),
                  (unsigned)(soma_b / n));
  }
  return n;
}

void stop() {
  if (!instalado_) return;
  i2s_stop(I2S_NUM_0);
  rodando_ = false;
}

bool running() { return rodando_; }

}  // namespace mic
