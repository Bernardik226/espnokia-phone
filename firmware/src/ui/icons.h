#pragma once
// Antena WiFi sem barras (F1 está offline) — mastro + 0 barras
static const unsigned char kIconWifiOff[] = {0x01, 0x01, 0x01, 0x01, 0x01, 0x07};
// Antena com 3 barras (usada a partir da F2; já fica pronta)
static const unsigned char kIconWifiOn[] = {0x21, 0x21, 0x29, 0x29, 0x2d, 0x2f};
// Bateria cheia (VBAT real só na F4)
static const unsigned char kIconBatt[] = {0x1e, 0x3f, 0x3f, 0x3f, 0x3f, 0x1e};
