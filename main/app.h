/*  app.h — l'etat du Macintosh miniature et ce qu'il expose.
 *  Aucun include ESP-IDF ici : app.cpp est le seul a parler au reseau.
 */
#ifndef APP_H
#define APP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* la version vient de VERSION, passee par main/CMakeLists.txt : elle ne peut
   plus diverger du fichier (c'etait le cas : 1.0.0 annonce pour 1.2.0) */
#ifndef OM_VERSION
#define OM_VERSION "1.2.1"
#endif

enum {
    VUE_BUREAU = 0,
    VUE_FINDER,
    VUE_WRITE,
    VUE_PAINT,
    VUE_TABLEAU,
    VUE_APROPOS,
    VUE_HORLOGE,
    VUE_SAFARI,
};

#define OM_ICONES 7

typedef struct {
    int  vue;                 /* bureau ou fenetre ouverte            */
    int  sel;                 /* icone choisie / ligne choisie        */
    int  page;                /* page interne (MacWrite, peinture)    */
    int  boot;                /* 0 disquette, 1 happy mac, 2 bureau   */
    int64_t boot_ms;
    bool wifi;
    bool heure_ok;
    int  rssi;
    int  essai_wifi;          /* 0 = bbox, 1 = son SSID Plus, 2 = 17pro */
    char heure[10];           /* "14:32"                              */
    char date[14];            /* "20/11"                              */
    uint32_t heap_libre;
    uint32_t psram_libre;
    int  uptime_min;
    float temp_c;
    int  bat_pct;             /* -1 : pas de jauge sur cette carte    */
    char ip[16];
    char ssid[24];
    char message[40];         /* bandeau temporaire                   */
    int  message_ms;
    unsigned rendu_ms;
    /* souris : position du pointeur, envoyee par le reseau (POSTE, curl) */
    int  mx, my;
    bool souris;
    /* SAFARI : le lecteur de pages en mode texte */
    int  saf_etat;            /* 0 jamais, 1 en cours, 2 pret, 3 erreur   */
    int  saf_code;            /* code HTTP                                */
    int  saf_lignes;
    int  saf_pages;
    int  saf_octets;
    int  saf_ms;
    char saf_err[28];
} app_t;

void app_start(const char *ssid, void (*ecran)(const uint8_t *fb, bool plein));
void app_geste(int btn, int type, int duree_ms);
void app_tick(void);               /* mesures + horloge, appele souvent */
void app_redessine(bool plein);    /* demande un rafraichissement       */
app_t *app_etat(void);
bool  app_plein_rafraichissement(void);
void  app_vue(int vue);            /* force une vue (API et tests)      */
void  app_bouton(int btn, int type, int duree_ms); /* alias de geste  */
void  app_clic(int x, int y);      /* clic a l'ecran, en pixels (0..199) */

#endif
