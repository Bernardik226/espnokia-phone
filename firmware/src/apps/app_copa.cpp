#include "app_copa.h"
#include <Arduino.h>
#include <string.h>
#include <U8g2lib.h>
#include "copamodel.h"
#include "i18n.h"
#include "jogo_aviso.h"
#include "net/http.h"
#include "net/wifi.h"
#include "sound.h"
#include "drivers/buzzer.h"
#include "drivers/rtc.h"
#include "ui/assets.h"
#include "ui/fonts3310.h"
#include "ui/goal_fx.h"
#include "ui/jogo_view.h"
#include "ui/nokia_ui.h"

// Copa 2026 via server companion: V_MENU escolhe a aba, V_LIST mostra a
// janela de 3 jogos (refeita em silencio enquanto houver jogo rolando, pra
// manter o placar da lista vivo), V_DETAIL abre um jogo em 3 paginas
// (placar | equipes | gols e estadio) e V_GRUPOS pagina a classificacao.
// O fetch HTTP
// e bloqueante (~ate 5 s), entao fica 1 frame em FETCH_PENDING pra tela
// "Buscando..." aparecer antes — o 3310 tambem "pensava" com a ampulheta.
enum View : uint8_t { V_MENU, V_LIST, V_DETAIL, V_GRUPOS };
enum Fetch : uint8_t { FETCH_IDLE, FETCH_PENDING, FETCH_OK, FETCH_ERR,
                       FETCH_NONET };

static const StrId kAbas[] = {STR_NEXT_GAMES, STR_BRAZIL, STR_LIVE_TAB,
                              STR_GROUPS};
static const char* kPaths[] = {"/copa/proximos?n=8", "/copa/brasil",
                               "/copa/live"};  // grupos tem rota propria
static const uint8_t kAbaCount = 4;
static const uint8_t kMaxJogos = 8;
static const uint8_t kMaxGrupos = 12;

static View view = V_MENU;
static Fetch fetch_ = FETCH_IDLE;
static uint8_t aba_ = 0;       // qual lista esta aberta
static uint8_t cur = 0;        // cursor (menu e lista)
static uint8_t pag_ = 0;  // detail: 0 placar, 1 equipes, 2 gols/estadio
static const uint8_t kPags = 3;
static uint8_t gcur_ = 0;      // grupo na tela em V_GRUPOS
static uint32_t pending_ms_ = 0;

static CopaJogo jogos_[kMaxJogos];
static uint8_t n_jogos_ = 0;
static CopaGrupo grupos_[kMaxGrupos];
static uint8_t n_grupos_ = 0;
static char buf_[3072];        // payload do server (/copa/grupos ~1.7 KB)

// acompanhamento ao vivo: o detail aberto refaz o GET e dispara o efeito de
// gol; a lista refaz o GET so pra manter o placar das linhas atualizado
static const uint32_t kLiveRefetchMs = 20000;
// ao ABRIR um jogo ao vivo, rele quase na hora: pega o gol que saiu entre o
// ultimo fetch da lista e agora (senao o efeito so dispararia em gol futuro)
static const uint32_t kAbreRefetchMs = 1500;
static int8_t live_s1_, live_s2_;
static char live_t1_[6], live_t2_[6];
static uint32_t live_next_ = 0;    // 0 = detail nao esta acompanhando
static uint32_t list_next_ = 0;    // 0 = lista sem jogo rolando

static bool alguma_live() {
  for (uint8_t i = 0; i < n_jogos_; i++)
    if (jogos_[i].live) return true;
  return false;
}

static void live_watch(const CopaJogo& j) {
  live_s1_ = j.s1; live_s2_ = j.s2;
  strncpy(live_t1_, j.t1, sizeof(live_t1_));
  strncpy(live_t2_, j.t2, sizeof(live_t2_));
  live_next_ = millis() + kAbreRefetchMs;  // rele quase na hora ao abrir
}

static void abre_lista(uint8_t aba) {
  aba_ = aba;
  cur = 0;
  // sem WiFi nem tenta o GET: mostra a tela padrao de sem conexao
  fetch_ = wifi::connected() ? FETCH_PENDING : FETCH_NONET;
  pending_ms_ = millis();
  view = V_LIST;
}

static void abre_grupos() {
  gcur_ = 0;
  fetch_ = wifi::connected() ? FETCH_PENDING : FETCH_NONET;
  pending_ms_ = millis();
  view = V_GRUPOS;
}

static void on_enter() {
  view = V_MENU;
  fetch_ = FETCH_IDLE;
  cur = 0;
  live_next_ = 0;
  list_next_ = 0;
}

// refetch silencioso do jogo aberto: reencontra pelo par de times (a lista
// do server pode mudar de ordem) e compara o placar pra acusar o gol
static void live_refetch(uint32_t now_ms) {
  live_next_ = now_ms + kLiveRefetchMs;
  if (http::get_json(kPaths[aba_], buf_, sizeof(buf_)) != 200) return;
  uint8_t n = copa_parse(buf_, jogos_, kMaxJogos, nullptr);
  if (n == 0) return;  // mantem o que tinha; tenta de novo no proximo ciclo
  n_jogos_ = n;
  for (uint8_t i = 0; i < n_jogos_; i++) {
    const CopaJogo& j = jogos_[i];
    if (strcmp(j.t1, live_t1_) != 0 || strcmp(j.t2, live_t2_) != 0) continue;
    cur = i;
    if (j.s1 > live_s1_ || j.s2 > live_s2_) {
      char placar[24];
      copa_linha(j, placar, sizeof(placar));
      const char* autor = (j.s1 > live_s1_) ? copa_ultimo_gol(j.g1)
                                            : copa_ultimo_gol(j.g2);
      goal_fx::start(placar, autor);
    }
    live_s1_ = j.s1; live_s2_ = j.s2;
    return;
  }
  view = V_LIST;  // o jogo saiu do feed (terminou): volta pra lista
  cur = 0;
  live_next_ = 0;
}

// refetch silencioso da lista: placar ao vivo nas linhas; quando o ultimo
// jogo rolando sai do feed, para de refazer sozinho
static void list_refetch(uint32_t now_ms) {
  list_next_ = now_ms + kLiveRefetchMs;
  if (http::get_json(kPaths[aba_], buf_, sizeof(buf_)) != 200) return;
  uint8_t n = copa_parse(buf_, jogos_, kMaxJogos, nullptr);
  if (n == 0) return;
  n_jogos_ = n;
  if (cur >= n_jogos_) cur = n_jogos_ - 1;
  if (!alguma_live()) list_next_ = 0;
}

static void tick(uint32_t now_ms) {
  // o GET bloqueante seguraria o PWM do bip de tecla ligado por segundos:
  // espera o som acabar (o buzzer::tick do main desliga) antes de buscar
  if (buzzer::busy()) return;
  if (view == V_DETAIL && live_next_ && (int32_t)(now_ms - live_next_) >= 0)
    live_refetch(now_ms);
  if (view == V_LIST && list_next_ && (int32_t)(now_ms - list_next_) >= 0)
    list_refetch(now_ms);
  // espera 1 frame (~50 ms) renderizar "Buscando..." antes de bloquear no GET
  if (fetch_ != FETCH_PENDING || now_ms - pending_ms_ < 60) return;
  if (view == V_GRUPOS) {
    int code = http::get_json("/copa/grupos", buf_, sizeof(buf_));
    n_grupos_ = (code == 200) ? copa_parse_grupos(buf_, grupos_, kMaxGrupos) : 0;
    fetch_ = (code == 200) ? FETCH_OK : FETCH_ERR;
    return;
  }
  int code = http::get_json(kPaths[aba_], buf_, sizeof(buf_));
  n_jogos_ = (code == 200) ? copa_parse(buf_, jogos_, kMaxJogos, nullptr) : 0;
  fetch_ = (code == 200) ? FETCH_OK : FETCH_ERR;  // erro fica na tela, sem bip
  list_next_ = (fetch_ == FETCH_OK && alguma_live()) ? now_ms + kLiveRefetchMs
                                                     : 0;
}

static bool input(Button b, BtnEvent e) {
  if (e != EV_PRESS) return false;
  switch (view) {
    case V_MENU:
      if (b == BTN_UP) { cur = (cur + kAbaCount - 1) % kAbaCount; return true; }
      if (b == BTN_DOWN) { cur = (cur + 1) % kAbaCount; return true; }
      if (b == BTN_OK) {
        if (cur == 3) abre_grupos();
        else abre_lista(cur);
        return true;
      }
      return false;  // C nao consumido → shell volta pro menu
    case V_LIST:
      if (fetch_ == FETCH_PENDING) return true;  // ocupado buscando
      if (fetch_ != FETCH_OK || n_jogos_ == 0) { view = V_MENU; cur = aba_; return true; }
      if (b == BTN_UP) { cur = (cur + n_jogos_ - 1) % n_jogos_; return true; }
      if (b == BTN_DOWN) { cur = (cur + 1) % n_jogos_; return true; }
      if (b == BTN_OK) {
        view = V_DETAIL;
        pag_ = 0;
        if (jogos_[cur].live) live_watch(jogos_[cur]);
        return true;
      }
      view = V_MENU; cur = aba_;  // C volta
      return true;
    case V_GRUPOS:
      if (fetch_ == FETCH_PENDING) return true;
      if (fetch_ == FETCH_OK && n_grupos_ > 0) {
        if (b == BTN_UP) { gcur_ = (gcur_ + n_grupos_ - 1) % n_grupos_; return true; }
        if (b == BTN_DOWN) { gcur_ = (gcur_ + 1) % n_grupos_; return true; }
      }
      view = V_MENU; cur = 3;  // OK/C volta
      return true;
    case V_DETAIL: {
      const CopaJogo& j = jogos_[cur];
      if (b == BTN_UP) { pag_ = (pag_ + kPags - 1) % kPags; return true; }
      if (b == BTN_DOWN) { pag_ = (pag_ + 1) % kPags; return true; }
      if (b == BTN_OK) {
        // OK alterna o par de avisos do jogo (inicio + gols, ver jogo_aviso)
        jogo_aviso_toggle(j, "/copa/live");
        sound::play(sound::SND_CONFIRM);
        return true;
      }
      view = V_LIST;  // C volta pra lista
      live_next_ = 0;
      return true;
    }
  }
  return false;
}

// dias no calendario civil (Howard Hinnant): diferenca de datas sem laco
static int32_t dias_civil(int y, int m, int d) {
  y -= m <= 2;
  int era = y / 400;
  int yoe = y - era * 400;
  int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (int32_t)era * 146097 + doe - 719468;
}

// contagem regressiva pra final (19/07/2026) no canto esquerdo, num
// minibanner preto com o texto vazado (contraste com o titulo); da final em
// diante vira so um "F". RTC desacertado (longe demais da Copa) esconde.
// Devolve a largura do banner (0 se nao desenhou nada).
static int desenha_final_em(U8G2& g) {
  rtc::DateTime dt;
  if (!rtc::now(dt)) return 0;
  int32_t falta =
      dias_civil(2026, 7, 19) - dias_civil(dt.year, dt.month, dt.day);
  if (falta > 60) return 0;
  char s[8];
  if (falta > 0)
    snprintf(s, sizeof(s), "%ldd", (long)falta);
  else
    snprintf(s, sizeof(s), "F");
  int w = (int)g.getStrWidth(s) + 4;
  g.drawBox(0, 0, w, 9);
  g.setDrawColor(0);
  g.drawStr(2, 8, s);
  g.setDrawColor(1);
  return w;
}

static void render(void* gfx) {
  U8G2& g = *(U8G2*)gfx;
  g.setFont(u8g2_font_3310_small);
  switch (view) {
    case V_MENU: {
      // titulo a direita; a contagem pra final mora no canto esquerdo
      const char* t = tr(STR_APP_COPA);
      int ini_t = 82 - nokia_ui::bold_width(g, t);
      nokia_ui::text_bold(g, ini_t, 8, t);
      int fim_b = desenha_final_em(g);
      // a taca da copa mora no respiro entre o banner e o titulo
      g.drawXBMP((fim_b + ini_t - MINI_CUP_W) / 2, 1, MINI_CUP_W, MINI_CUP_H,
                 mini_cup_bits);
      const uint8_t kVis = 3;
      uint8_t top = cur >= kVis ? (uint8_t)(cur - kVis + 1) : 0;
      for (uint8_t i = 0; i < kVis && top + i < kAbaCount; i++) {
        int y = 11 + i * 9;
        nokia_ui::list_row(g, y, 84, tr(kAbas[top + i]), top + i == cur);
      }
      nokia_ui::softkey(g, tr(STR_SELECT));
      break;
    }
    case V_LIST: {
      nokia_ui::text_bold_center(g, 8, tr(kAbas[aba_]));
      if (fetch_ == FETCH_PENDING) {
        g.drawUTF8(2, 24, tr(STR_SEARCHING));
        break;
      }
      if (fetch_ == FETCH_NONET) {
        nokia_ui::no_network(g);
        nokia_ui::softkey(g, tr(STR_BACK));
        break;
      }
      if (fetch_ == FETCH_ERR) {
        nokia_ui::text_bold_center(g, 24, tr(STR_NO_RESPONSE));
        nokia_ui::softkey(g, tr(STR_BACK));
        break;
      }
      if (n_jogos_ == 0) {
        g.drawUTF8(2, 24, tr(STR_NO_GAMES));
        nokia_ui::softkey(g, tr(STR_BACK));
        break;
      }
      const uint8_t kVis = 3;
      uint8_t top = cur >= kVis ? (uint8_t)(cur - kVis + 1) : 0;
      char linha[24];
      for (uint8_t i = 0; i < kVis && top + i < n_jogos_; i++) {
        int y = 11 + i * 9;
        const CopaJogo& j = jogos_[top + i];
        copa_linha(j, linha, sizeof(linha));
        bool sel = (top + i == cur);
        nokia_ui::list_row(g, y, 84, linha, sel);
        if (sel) g.setDrawColor(0);
        if (j.live)  // bolinha de "ao vivo"
          g.drawStr(78, y + 8, "*");
        else if (j.s1 >= 0)  // reloginho: este ja e historico
          g.drawXBMP(78, y + 2, MINI_CLOCK_W, MINI_CLOCK_H, mini_clock_bits);
        if (sel) g.setDrawColor(1);
      }
      nokia_ui::softkey(g, tr(STR_OPEN));
      break;
    }
    case V_DETAIL: {
      const CopaJogo& j = jogos_[cur];
      jogo_view_detail(g, j, pag_, jogo_aviso_armado(j));
      break;
    }
    case V_GRUPOS: {
      if (fetch_ == FETCH_PENDING) {
        nokia_ui::text_bold_center(g, 8, tr(STR_GROUPS));
        g.drawUTF8(2, 24, tr(STR_SEARCHING));
        break;
      }
      if (fetch_ == FETCH_NONET) {
        nokia_ui::text_bold_center(g, 8, tr(STR_GROUPS));
        nokia_ui::no_network(g);
        nokia_ui::softkey(g, tr(STR_BACK));
        break;
      }
      if (fetch_ != FETCH_OK || n_grupos_ == 0) {
        nokia_ui::text_bold_center(g, 8, tr(STR_GROUPS));
        nokia_ui::text_bold_center(g, 24, tr(STR_NO_RESPONSE));
        nokia_ui::softkey(g, tr(STR_BACK));
        break;
      }
      const CopaGrupo& grp = grupos_[gcur_];
      char titulo[20];
      snprintf(titulo, sizeof(titulo), "%s %s", tr(STR_GROUP), grp.nome);
      nokia_ui::text_bold_center(g, 8, titulo);
      // colunas: pontos, jogos, saldo de gols (classificado pelo server)
      g.drawUTF8(34, 16, tr(STR_TBL_PTS));
      g.drawUTF8(50, 16, tr(STR_TBL_JOGOS));
      g.drawUTF8(62, 16, tr(STR_TBL_SALDO));
      char num[8];
      for (uint8_t i = 0; i < grp.nt; i++) {
        int y = 24 + i * 7;
        const CopaGrupoTime& t = grp.t[i];
        g.drawStr(2, y, t.c);
        snprintf(num, sizeof(num), "%d", t.pts);
        g.drawStr(34, y, num);
        snprintf(num, sizeof(num), "%d", t.j);
        g.drawStr(50, y, num);
        snprintf(num, sizeof(num), "%+d", t.sg);
        g.drawStr(62, y, num);
      }
      break;
    }
  }
}

// animacao de selecao: sparkle percorrendo o trofeu
static const unsigned char* const kAnim[] = {icon_copa_f1_bits,
                                             icon_copa_f2_bits, nullptr};
const App app_copa = {STR_APP_COPA, on_enter, tick, input, nullptr, render,
                      icon_copa_bits, kAnim};
