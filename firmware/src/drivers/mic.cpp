#include "mic.h"

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
  cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = 0;
  cfg.dma_buf_count = 6;     // 6x256 amostras ~ 96 ms de folga pro loop
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
  static int32_t cru[256];
  size_t pedir = max < 256 ? max : 256;
  size_t lidos = 0;
  if (i2s_read(I2S_NUM_0, cru, pedir * sizeof(int32_t), &lidos, 0) != ESP_OK)
    return 0;
  size_t n = lidos / sizeof(int32_t);
  for (size_t i = 0; i < n; i++) {
    int32_t v = cru[i] >> 14;  // 24 bits uteis -> PCM16 com ganho ~4x
    if (v > 32767) v = 32767;
    if (v < -32768) v = -32768;
    buf[i] = (int16_t)v;
  }
  return n;
}

void stop() {
  if (!instalado_) return;
  i2s_stop(I2S_NUM_0);
  rodando_ = false;
}

}  // namespace mic
