#include "timeprefs.h"
#include <Preferences.h>

namespace timeprefs {

static Preferences prefs_;
static bool open_ = false;
static bool fmt24_ = true;
static bool show_date_ = false;

static void ensure() {
  if (open_) return;
  prefs_.begin("espnokia");   // mesmo namespace do resto do sistema
  fmt24_ = prefs_.getBool("fmt24", true);
  show_date_ = prefs_.getBool("showdate", false);
  open_ = true;
}

void init() { ensure(); }

bool fmt24() { ensure(); return fmt24_; }
void set_fmt24(bool v) { ensure(); fmt24_ = v; prefs_.putBool("fmt24", v); }

bool show_date() { ensure(); return show_date_; }
void set_show_date(bool v) { ensure(); show_date_ = v; prefs_.putBool("showdate", v); }

}  // namespace timeprefs
