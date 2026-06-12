#include "teams.h"
#include <string.h>

// As 48 selecoes da Copa 2026, uma coluna por idioma (PT EN ES FR DE FI).
// Mesma fonte de verdade do server (codigos FIFA); strings UTF-8.
struct TeamRow {
  const char* code;
  const char* name[LANG_COUNT];
};

static const TeamRow kTeams[] = {
  {"ALG", {"Argélia", "Algeria", "Argelia", "Algérie", "Algerien",
           "Algeria"}},
  {"ARG", {"Argentina", "Argentina", "Argentina", "Argentine",
           "Argentinien", "Argentiina"}},
  {"AUS", {"Austrália", "Australia", "Australia", "Australie",
           "Australien", "Australia"}},
  {"AUT", {"Áustria", "Austria", "Austria", "Autriche", "Österreich",
           "Itävalta"}},
  {"BEL", {"Bélgica", "Belgium", "Bélgica", "Belgique", "Belgien",
           "Belgia"}},
  {"BIH", {"Bósnia", "Bosnia", "Bosnia", "Bosnie", "Bosnien", "Bosnia"}},
  {"BRA", {"Brasil", "Brazil", "Brasil", "Brésil", "Brasilien",
           "Brasilia"}},
  {"CAN", {"Canadá", "Canada", "Canadá", "Canada", "Kanada", "Kanada"}},
  {"CIV", {"Costa do Marfim", "Ivory Coast", "Costa de Marfil",
           "Côte d'Ivoire", "Elfenbeinküste", "Norsunluurann."}},
  {"COD", {"RD Congo", "DR Congo", "RD Congo", "RD Congo", "DR Kongo",
           "Kongon DT"}},
  {"COL", {"Colômbia", "Colombia", "Colombia", "Colombie", "Kolumbien",
           "Kolumbia"}},
  {"CPV", {"Cabo Verde", "Cape Verde", "Cabo Verde", "Cap-Vert",
           "Kap Verde", "Kap Verde"}},
  {"CRO", {"Croácia", "Croatia", "Croacia", "Croatie", "Kroatien",
           "Kroatia"}},
  {"CUW", {"Curaçao", "Curaçao", "Curazao", "Curaçao", "Curaçao",
           "Curaçao"}},
  {"CZE", {"Tchéquia", "Czechia", "Chequia", "Tchéquie", "Tschechien",
           "Tšekki"}},
  {"ECU", {"Equador", "Ecuador", "Ecuador", "Équateur", "Ecuador",
           "Ecuador"}},
  {"EGY", {"Egito", "Egypt", "Egipto", "Égypte", "Ägypten", "Egypti"}},
  {"ENG", {"Inglaterra", "England", "Inglaterra", "Angleterre", "England",
           "Englanti"}},
  {"ESP", {"Espanha", "Spain", "España", "Espagne", "Spanien", "Espanja"}},
  {"FRA", {"França", "France", "Francia", "France", "Frankreich",
           "Ranska"}},
  {"GER", {"Alemanha", "Germany", "Alemania", "Allemagne", "Deutschland",
           "Saksa"}},
  {"GHA", {"Gana", "Ghana", "Ghana", "Ghana", "Ghana", "Ghana"}},
  {"HAI", {"Haiti", "Haiti", "Haití", "Haïti", "Haiti", "Haiti"}},
  {"IRN", {"Irã", "Iran", "Irán", "Iran", "Iran", "Iran"}},
  {"IRQ", {"Iraque", "Iraq", "Irak", "Irak", "Irak", "Irak"}},
  {"JOR", {"Jordânia", "Jordan", "Jordania", "Jordanie", "Jordanien",
           "Jordania"}},
  {"JPN", {"Japão", "Japan", "Japón", "Japon", "Japan", "Japani"}},
  {"KOR", {"Coreia do Sul", "South Korea", "Corea del Sur",
           "Corée du Sud", "Südkorea", "Etelä-Korea"}},
  {"KSA", {"Arábia Saudita", "Saudi Arabia", "Arabia Saudita",
           "Arabie saoudite", "Saudi-Arabien", "Saudi-Arabia"}},
  {"MAR", {"Marrocos", "Morocco", "Marruecos", "Maroc", "Marokko",
           "Marokko"}},
  {"MEX", {"México", "Mexico", "México", "Mexique", "Mexiko", "Meksiko"}},
  {"NED", {"Holanda", "Netherlands", "Países Bajos", "Pays-Bas",
           "Niederlande", "Alankomaat"}},
  {"NOR", {"Noruega", "Norway", "Noruega", "Norvège", "Norwegen",
           "Norja"}},
  {"NZL", {"Nova Zelândia", "New Zealand", "Nueva Zelanda",
           "Nouvelle-Zélande", "Neuseeland", "Uusi-Seelanti"}},
  {"PAN", {"Panamá", "Panama", "Panamá", "Panama", "Panama", "Panama"}},
  {"PAR", {"Paraguai", "Paraguay", "Paraguay", "Paraguay", "Paraguay",
           "Paraguay"}},
  {"POR", {"Portugal", "Portugal", "Portugal", "Portugal", "Portugal",
           "Portugali"}},
  {"QAT", {"Catar", "Qatar", "Catar", "Qatar", "Katar", "Qatar"}},
  {"RSA", {"África do Sul", "South Africa", "Sudáfrica",
           "Afrique du Sud", "Südafrika", "Etelä-Afrikka"}},
  {"SCO", {"Escócia", "Scotland", "Escocia", "Écosse", "Schottland",
           "Skotlanti"}},
  {"SEN", {"Senegal", "Senegal", "Senegal", "Sénégal", "Senegal",
           "Senegal"}},
  {"SUI", {"Suíça", "Switzerland", "Suiza", "Suisse", "Schweiz",
           "Sveitsi"}},
  {"SWE", {"Suécia", "Sweden", "Suecia", "Suède", "Schweden", "Ruotsi"}},
  {"TUN", {"Tunísia", "Tunisia", "Túnez", "Tunisie", "Tunesien",
           "Tunisia"}},
  {"TUR", {"Turquia", "Turkey", "Turquía", "Turquie", "Türkei", "Turkki"}},
  {"URU", {"Uruguai", "Uruguay", "Uruguay", "Uruguay", "Uruguay",
           "Uruguay"}},
  {"USA", {"Estados Unidos", "United States", "Estados Unidos",
           "États-Unis", "USA", "Yhdysvallat"}},
  {"UZB", {"Uzbequistão", "Uzbekistan", "Uzbekistán", "Ouzbékistan",
           "Usbekistan", "Uzbekistan"}},
};

const char* team_name(const char* fifa_code) {
  for (const TeamRow& t : kTeams)
    if (strcmp(t.code, fifa_code) == 0) return t.name[i18n_lang()];
  return fifa_code;
}

uint8_t team_count() { return sizeof(kTeams) / sizeof(kTeams[0]); }
