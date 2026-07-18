#pragma once
#include <stdint.h>

// Idiomas do sistema. Tabelas const em flash; tr() resolve pelo idioma
// corrente. A lib e pura (sem Arduino): a persistencia do idioma escolhido
// fica no glue (main carrega da NVS no boot, settings salva ao trocar).
// Strings com acento sao UTF-8: renderizar com drawUTF8, nunca drawStr.
enum Lang : uint8_t { LANG_PT, LANG_EN, LANG_ES, LANG_FR, LANG_DE, LANG_FI,
                      LANG_COUNT };

enum StrId : uint8_t {
  STR_NONE,  // string vazia (apps que nao aparecem no launcher)
  // nomes de app (App.name_id)
  STR_APP_CLOCK, STR_APP_WEATHER, STR_APP_TONES, STR_APP_SETTINGS,
  STR_APP_COPA,
  // softkeys e acoes comuns
  STR_MENU, STR_BACK, STR_OK, STR_SELECT, STR_OPEN, STR_SAVE, STR_CHANGE,
  STR_PLAY, STR_STOP, STR_OPTIONS,
  // configuracoes
  STR_DISPLAY, STR_BACKLIGHT, STR_DATETIME, STR_WIFI, STR_LANGUAGE,
  STR_ABOUT, STR_SYSTEM, STR_CONFIG_MODE, STR_PASS_FMT, STR_CONNECTING,
  STR_FORGET_NET, STR_SWITCH_NET, STR_SET_NOW, STR_SYNC, STR_NO_RTC,
  STR_RAM_FREE_FMT,
  // copa
  STR_NEXT_GAMES, STR_BRAZIL, STR_LIVE_TAB, STR_SEARCHING, STR_NO_RESPONSE,
  STR_NO_GAMES, STR_LIVE_BIG, STR_NOTIFY, STR_NOTIFY_OFF, STR_NOTIFY_ON,
  STR_GAME_NOW,
  // clima
  STR_INT_SENSOR, STR_NO_SENSOR,
  // rede
  STR_CONNECT_WIFI,
  // som
  STR_VOLUME, STR_VOL_LOW, STR_VOL_MED, STR_VOL_HIGH,
  // relogio (alarme/timer) e avisos
  STR_ALARM, STR_TIMER, STR_TIME_UP, STR_TURN_OFF, STR_GOAL,
  // copa v2 (novas strings sempre no fim, antes de STR_COUNT)
  STR_GROUPS, STR_GOALS, STR_GROUP,
  // categoria esportes
  STR_APP_SPORTS, STR_APP_FUTEBOL,
  // app claude
  STR_APP_CLAUDE, STR_TALK, STR_LISTENING, STR_THINKING, STR_SAY_AGAIN,
  // registro de conversa do claude
  STR_NO_TALKS, STR_ME,
  // tabela do futebol (submenu da liga + titulo da tabela unica)
  STR_GAMES, STR_TABLE,
  // conexoes (menu de settings + QR de pareamento do dashboard)
  STR_CONNECTIONS, STR_WIFI_SERVER, STR_DEVICE_QR, STR_SCAN_TO_PAIR,
  // jogos
  STR_APP_GAMES, STR_GAME_SNAKE, STR_LEVEL, STR_GAME_OVER, STR_RECORD,
  STR_NEW_RECORD, STR_NEW_GAME,
  // niveis do backlight (config > visor)
  STR_DISP_OFF, STR_DISP_MED, STR_DISP_HIGH,
  // cabecalhos de tabela (copa/futebol): pontos, jogos, saldo de gols.
  // curtos de proposito (colunas de ~12-16px) - ver getStrWidth() checado
  // por fonte real em fonts3310.h antes de escolher as abreviacoes.
  STR_TBL_PTS, STR_TBL_JOGOS, STR_TBL_SALDO,
  // sufixo de unidade do timer do relogio (editor de minutos)
  STR_MIN_SUFFIX,
  // jogos novos (2048, breakout) e tela de vitoria
  STR_GAME_2048, STR_GAME_BREAKOUT, STR_YOU_WIN,
  // menu do jogo (iniciar / som / mudo)
  STR_START, STR_SOUND, STR_MUTE,
  // wallpaper da tela inicial (config do sistema)
  STR_WALLPAPER, STR_WALL_NEUTRO, STR_WALL_PONTOS,
  // cronometro do app relogio
  STR_STOPWATCH, STR_RESET,
  // alarme (quanto falta) e toggle de mostrar a data no menu inicial
  STR_RINGS_IN, STR_SHOW_DATE,
  // formato 12h/AM-PM do sistema (Config>Data e hora)
  STR_AMPM,
  STR_COUNT
};

const char* tr(StrId id);            // string no idioma corrente
const char* day_name(uint8_t dow);   // 0 = domingo
const char* lang_name(Lang l);       // nome do idioma, no proprio idioma
void i18n_set(Lang l);               // fora do range vira LANG_PT
Lang i18n_lang();
