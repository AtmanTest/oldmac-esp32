/*  ui.cpp — le Systeme du Macintosh miniature, dessine en 1 bit.
 *
 *  Tout passe par art.h : aucune dependance ESP-IDF, donc le meme dessin
 *  peut etre verifie sur le PC et lu en texte par l'API (/ecran).
 *
 *  Reperes de la dalle 200 x 200 :
 *    0..12   barre de menus (pomme, Fichier, Edition, Special, horloge)
 *    13..181 le bureau (trame 50 %) avec ses icones et la fenetre ouverte
 *    182..199 le pied : les deux boutons, toujours rappeles
 */
#include "ui.h"
#include "safari.h"
#include "art.h"
#include <stdio.h>
#include <string.h>

static const int IC_X[OM_ICONES] = {  8,  8,   8, 104, 104, 104, 168 };
static const int IC_Y[OM_ICONES] = { 24, 72, 120,  24,  72, 120,  72 };
static const char *IC_NOM[OM_ICONES] = { "Disque", "MacWrite", "MacPaint",
                                         "Tableau", "Horloge", "A propos",
                                         "Safari" };
static const int IC_VUE[OM_ICONES] = { VUE_FINDER, VUE_WRITE, VUE_PAINT,
                                       VUE_TABLEAU, VUE_HORLOGE, VUE_APROPOS,
                                       VUE_SAFARI };

int ui_icone_x(int i) { return (i >= 0 && i < OM_ICONES) ? IC_X[i] : 0; }
int ui_icone_y(int i) { return (i >= 0 && i < OM_ICONES) ? IC_Y[i] : 0; }
const char *ui_icone_nom(int i) { return (i >= 0 && i < OM_ICONES) ? IC_NOM[i] : ""; }
int ui_vue_pour_icone(int i) { return (i >= 0 && i < OM_ICONES) ? IC_VUE[i] : VUE_BUREAU; }
int ui_lignes_finder(void) { return 6; }

/* ------------------------------------------------------------------ */
/* primitives                                                         */
/* ------------------------------------------------------------------ */
static void rempli(int x, int y, int w, int h, int noir)
{
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) a_px(x + i, y + j, noir);
}

static void cadre(int x, int y, int w, int h)
{
    a_hline(x, x + w - 1, y, 1);
    a_hline(x, x + w - 1, y + h - 1, 1);
    a_vline(x, y, y + h - 1, 1);
    a_vline(x + w - 1, y, y + h - 1, 1);
}

/* trame 25 % : le gris du bureau, assez clair pour que le texte reste lisible */
static void trame(int x, int y, int w, int h)
{
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            if (((x + i) & 1) == 0 && ((y + j) & 1) == 0) a_px(x + i, y + j, 1);
}


static void texte(int x, int y, int s, const char *t) { a_text(x, y, s, t); }

static void texte_droite(int x_droite, int y, int s, const char *t)
{
    int w = a_text_w(s, t);
    texte(x_droite - w, y, s, t);
}

static void texte_centre(int cx, int y, int s, const char *t)
{
    int w = a_text_w(s, t);
    texte(cx - w / 2, y, s, t);
}
/* une etiquette d'icone : fond blanc, sinon la trame mange le texte */
static void etiquette(int cx, int y, const char *t)
{
    int w = a_text_w(1, t);
    rempli(cx - w / 2 - 1, y - 1, w + 2, 7, 0);
    texte_centre(cx, y, 1, t);
}

/* une ligne d'item, avec barre de selection si choisie */
static void item(int x, int y, int w, int s, const char *t, int choisi)
{
    int h = 5 * s;
    if (choisi) {
        rempli(x - 2, y - 2, w + 4, h + 4, 1);
        a_text_col(x, y, s, t, 0);
    } else {
        texte(x, y, s, t);
    }
}

/* ------------------------------------------------------------------ */
/* icones                                                             */
/* ------------------------------------------------------------------ */
static void icone(int i, int x, int y, int choisi)
{
    int cx = x + 10, cy = y + 8;
    if (choisi) {
        rempli(x - 3, y - 3, 26, 26, 1);
        a_hline(x - 4, x + 23, y - 4, 1);
        a_hline(x - 4, x + 23, y + 23, 1);
        a_vline(x - 4, y - 4, y + 23, 1);
        a_vline(x + 23, y - 4, y + 23, 1);
        rempli(x - 2, y - 2, 24, 24, 0);
    }
    switch (i) {
    case 0:   /* un disque : boitier et fente */
        a_ring(cx, cy, 7, 1, 1);
        a_hline(x + 5, x + 15, y + 12, 1);
        a_px(x + 14, y + 12, 0);
        break;
    case 1:   /* un document */
    case 2:   /* un document avec un pinceau */
        a_hline(x + 4, x + 16, y + 2, 1);
        a_vline(x + 4, y + 2, y + 15, 1);
        a_vline(x + 16, y + 2, y + 15, 1);
        a_hline(x + 4, x + 16, y + 15, 1);
        a_hline(x + 7, x + 13, y + 6, 1);
        a_hline(x + 7, x + 13, y + 9, 1);
        a_hline(x + 7, x + 11, y + 12, 1);
        if (i == 2) a_seg(x + 5, y + 14, x + 14, y + 4, 1);
        break;
    case 3:   /* un tableau de bord : deux curseurs */
        a_hline(x + 4, x + 16, y + 2, 1);
        a_vline(x + 4, y + 2, y + 15, 1);
        a_vline(x + 16, y + 2, y + 15, 1);
        a_hline(x + 4, x + 16, y + 15, 1);
        a_hline(x + 7, x + 13, y + 7, 1);
        a_vline(x + 10, y + 5, y + 9, 1);
        a_hline(x + 7, x + 13, y + 11, 1);
        a_vline(x + 8, y + 9, y + 13, 1);
        break;
    case 6:   /* une boussole : anneau et aiguille */
        a_ring(cx, cy, 7, 1, 1);
        a_seg(x + 6, y + 13, x + 14, y + 3, 1);
        a_seg(x + 10, y + 9, x + 14, y + 3, 1);
        a_px(cx, cy, 1);
        break;
    case 4:   /* une horloge */
        a_ring(cx, cy, 7, 1, 1);
        a_seg(cx, cy, cx, cy - 4, 1);
        a_seg(cx, cy, cx + 3, cy + 1, 1);
        break;
    default:  /* a propos : un petit Macintosh */
        a_hline(x + 3, x + 17, y + 2, 1);
        a_vline(x + 3, y + 2, y + 13, 1);
        a_vline(x + 17, y + 2, y + 13, 1);
        a_hline(x + 3, x + 17, y + 13, 1);
        a_hline(x + 6, x + 14, y + 5, 1);
        a_hline(x + 6, x + 14, y + 10, 1);
        a_hline(x + 7, x + 13, y + 16, 1);
        break;
    }
}

static void corbeille(int x, int y, int choisi)
{
    if (choisi) {
        rempli(x - 3, y - 3, 26, 26, 1);
        rempli(x - 2, y - 2, 24, 24, 0);
    }
    a_hline(x + 4, x + 16, y + 2, 1);
    a_vline(x + 5, y + 4, y + 16, 1);
    a_vline(x + 15, y + 4, y + 16, 1);
    a_hline(x + 5, x + 15, y + 16, 1);
    a_vline(x + 9, y + 6, y + 14, 1);
    a_vline(x + 12, y + 6, y + 14, 1);
}

/* ------------------------------------------------------------------ */
/* barre de menus                                                     */
/* ------------------------------------------------------------------ */
static void barre_menus(const app_t *s)
{
    rempli(0, 0, OM_W, OM_MB, 0);          /* bandeau blanc */
    a_hline(0, OM_W - 1, OM_MB - 1, 1);    /* trait de separation */

    /* la pomme : deux bosses, une morsure, une feuille */
    a_disc(6, 5, 3, 1);
    a_disc(10, 5, 3, 1);
    a_hline(4, 12, 7, 1);
    a_disc(12, 6, 2, 0);
    a_px(9, 1, 1);
    a_px(10, 0, 1);

    texte(16, 4, 1, "Fichier");
    texte(48, 4, 1, "Edition");
    texte(78, 4, 1, "Special");

    /* etat du reseau, puis l'heure, a droite */
    if (s->wifi) {
        a_hline(148, 150, 9, 1);
        a_hline(151, 153, 7, 1);
        a_hline(154, 156, 5, 1);
        a_px(155, 10, 1);
    } else {
        for (int i = 0; i < 6; i++) a_px(150 + i, 5 + i, 1), a_px(155 - i, 5 + i, 1);
    }
    texte_droite(OM_W - 3, 4, 1, s->heure);
}

/* ------------------------------------------------------------------ */
/* fenetre                                                            */
/* ------------------------------------------------------------------ */
static void fenetre(const char *titre)
{
    rempli(OM_WIN_X, OM_WIN_Y, OM_WIN_W, OM_WIN_H, 0);
    cadre(OM_WIN_X, OM_WIN_Y, OM_WIN_W, OM_WIN_H);

    /* barre de titre a rayures, case de fermeture, titre */
    for (int y = OM_WIN_Y + 2; y < OM_WIN_Y + OM_TITRE_H; y += 2)
        a_hline(OM_WIN_X + 12, OM_WIN_X + OM_WIN_W - 4, y, 1);
    rempli(OM_WIN_X + 12, OM_WIN_Y + 2, 8, 7, 1);
    a_hline(OM_WIN_X + 12, OM_WIN_X + 19, OM_WIN_Y + 8, 1);
    a_vline(OM_WIN_X + 19, OM_WIN_Y + 2, OM_WIN_Y + 8, 1);
    int w = a_text_w(1, titre);
    rempli(OM_WIN_X + OM_WIN_W / 2 - w / 2 - 2, OM_WIN_Y + 2, w + 4, 7, 0);
    texte_centre(OM_WIN_X + OM_WIN_W / 2, OM_WIN_Y + 3, 1, titre);
    a_hline(OM_WIN_X + 1, OM_WIN_X + OM_WIN_W - 2, OM_WIN_Y + OM_TITRE_H, 1);
}

/* ------------------------------------------------------------------ */
/* les applications                                                   */
/* ------------------------------------------------------------------ */
static void vue_finder(const app_t *s)
{
    fenetre("Macintosh HD");
    int x = OM_WIN_X + 6, y = OM_WIN_Y + OM_TITRE_H + 5;
    char l[48];
    texte(x, y, 1, "6 elements, 800 Ko");
    y += 9;
    const char *items[6];
    char t2[32], t3[32], t4[32];
    items[0] = "Macintosh HD";
    items[1] = "Systeme 6.0.8";
    snprintf(t2, sizeof(t2), "memoire : %u Ko", (unsigned)(s->heap_libre / 1024));
    items[2] = t2;
    snprintf(t3, sizeof(t3), "wifi : %s", s->wifi ? s->ssid : "hors ligne");
    items[3] = t3;
    if (s->temp_c > -100) snprintf(t4, sizeof(t4), "puce : %.1f C", (double)s->temp_c);
    else snprintf(t4, sizeof(t4), "puce : n.d.");
    items[4] = t4;
    items[5] = "oldmac " OM_VERSION;
    for (int i = 0; i < 6; i++) {
        item(x, y + i * 12, OM_WIN_W - 14, 1, items[i], s->sel == i);
    }
    texte(x, OM_WIN_Y + OM_WIN_H - 12, 1, "B2 choisit, B1 ouvre");
}

static void vue_write(const app_t *s)
{
    fenetre("MacWrite");
    static const char *pages[3][6] = {
        { "GOOD BYE....... DAD!", "",
          "CE MACINTOSH A 39 ANS", "ET IL DEMARRE ENCORE.",
          "", "IL SE SOUVIENT DE TOI." },
        { "DE LA MAISON, DU CHIEN", "QUI DISAIT AU REVOIR",
          "SUR L'ECRAN.", "",
          "B2 : PAGE SUIVANTE", "B1 : FERMER" },
        { "TOUT CE QUE TU LIS EST", "DESSINE ICI, EN 1 BIT,",
          "PAR CE PROGRAMME.", "",
          "AUCUNE IMAGE, AUCUNE", "POLICE D'APPLE." },
    };
    int p = s->page % 3;
    int x = OM_WIN_X + 8, y = OM_WIN_Y + OM_TITRE_H + 6;
    for (int i = 0; i < 6; i++) {
        texte(x, y + i * 11, 2, pages[p][i]);
    }
    /* curseur : il clignote, ici il est toujours la */
    char l[24];
    snprintf(l, sizeof(l), "page %d sur 3", p + 1);
    texte(x, OM_WIN_Y + OM_WIN_H - 12, 1, l);
}

static void vue_paint(const app_t *s)
{
    fenetre("MacPaint");
    int ox = OM_WIN_X + 10, oy = OM_WIN_Y + OM_TITRE_H + 12;
    if ((s->page & 1) == 0) {
        /* le chien de la photo */
        a_ring(ox + 26, oy + 26, 17, 1, 1);
        a_ring(ox + 26, oy + 26, 16, 1, 1);
        a_disc(ox + 14, oy + 10, 6, 1);
        a_disc(ox + 38, oy + 10, 6, 1);
        a_disc(ox + 14, oy + 10, 4, 0);
        a_disc(ox + 38, oy + 10, 4, 0);
        a_disc(ox + 20, oy + 22, 2, 1);
        a_disc(ox + 32, oy + 22, 2, 1);
        a_disc(ox + 26, oy + 33, 4, 1);
        a_hline(ox + 20, ox + 32, oy + 41, 1);
        a_hline(ox + 8, ox + 16, oy + 30, 1);
        a_hline(ox + 8, ox + 16, oy + 34, 1);
        a_hline(ox + 36, ox + 44, oy + 30, 1);
        a_hline(ox + 36, ox + 44, oy + 34, 1);
        texte(ox + 54, oy + 8, 1, "good bye.......");
        texte(ox + 84, oy + 16, 1, "dad!");
    } else {
        /* l'os, pour les jours heureux */
        a_hline(ox + 14, ox + 78, oy + 26, 1);
        a_hline(ox + 14, ox + 78, oy + 32, 1);
        a_vline(ox + 14, oy + 26, oy + 32, 1);
        a_vline(ox + 78, oy + 26, oy + 32, 1);
        a_disc(ox + 12, oy + 23, 5, 1);
        a_disc(ox + 12, oy + 35, 5, 1);
        a_disc(ox + 80, oy + 23, 5, 1);
        a_disc(ox + 80, oy + 35, 5, 1);
        a_disc(ox + 12, oy + 29, 3, 0);
        a_disc(ox + 80, oy + 29, 3, 0);
        texte(ox + 20, oy + 4, 1, "les jours heureux");
    }
    texte(OM_WIN_X + 8, OM_WIN_Y + OM_WIN_H - 12, 1,
          (s->page & 1) ? "B2 : le chien" : "B2 : l'os");
}

static void vue_tableau(const app_t *s)
{
    fenetre("Tableau de bord");
    int x = OM_WIN_X + 8, y = OM_WIN_Y + OM_TITRE_H + 6;
    char l[48];
    texte(x, y, 1, "Cette machine, pour de vrai");
    y += 10;
    snprintf(l, sizeof(l), "memoire libre : %u Ko", (unsigned)(s->heap_libre / 1024));
    texte(x, y, 1, l); y += 8;
    snprintf(l, sizeof(l), "PSRAM : %u Ko", (unsigned)(s->psram_libre / 1024));
    texte(x, y, 1, l); y += 8;
    snprintf(l, sizeof(l), "en marche : %d min", s->uptime_min);
    texte(x, y, 1, l); y += 8;
    if (s->wifi) {
        snprintf(l, sizeof(l), "%s  %d dBm", s->ssid, s->rssi);
        texte(x, y, 1, l); y += 8;
        snprintf(l, sizeof(l), "adresse : %s", s->ip);
        texte(x, y, 1, l); y += 8;
    } else {
        texte(x, y, 1, "wifi : hors ligne"); y += 8;
        texte(x, y, 1, "adresse : n.d."); y += 8;
    }
    if (s->temp_c > -100) snprintf(l, sizeof(l), "puce : %.1f C", (double)s->temp_c);
    else snprintf(l, sizeof(l), "puce : n.d.");
    texte(x, y, 1, l); y += 10;
    texte(x, y, 1, "heure :");
    texte(x + a_text_w(1, "heure : ") + 1, y, 1,
          s->heure_ok ? "internet" : "non synchronisee");
    texte(x, OM_WIN_Y + OM_WIN_H - 12, 1, "B2 : remesurer");
}

static void vue_apropos(const app_t *s)
{
    (void)s;
    fenetre("A propos");
    int ox = OM_WIN_X + 10, oy = OM_WIN_Y + OM_TITRE_H + 8;
    /* le Mac en miniature */
    cadre(ox, oy, 46, 38);
    cadre(ox + 4, oy + 4, 38, 24);
    texte(ox + 6, oy + 8, 1, "hello");
    a_hline(ox + 8, ox + 36, oy + 32, 1);
    texte(ox + 54, oy + 2, 2, "Macintosh");
    texte(ox + 54, oy + 16, 1, "128K simule");
    texte(ox + 54, oy + 24, 1, "Systeme 6.0.8");
    texte(ox + 54, oy + 32, 1, "ecran 200x200");
    texte(OM_WIN_X + 8, oy + 46, 1, "oldmac-esp32 " OM_VERSION);
    texte(OM_WIN_X + 8, oy + 56, 1, "@paristzou");
    texte(OM_WIN_X + 8, oy + 64, 1, "juste pour les filles");
    texte(OM_WIN_X + 8, oy + 78, 1, "B1 : fermer");
}

static void vue_horloge(const app_t *s)
{
    fenetre("Horloge");
    texte_centre(OM_WIN_X + OM_WIN_W / 2, OM_WIN_Y + OM_TITRE_H + 22, 5, s->heure);
    texte_centre(OM_WIN_X + OM_WIN_W / 2, OM_WIN_Y + OM_TITRE_H + 60, 2, s->date);
    texte_centre(OM_WIN_X + OM_WIN_W / 2, OM_WIN_Y + OM_TITRE_H + 84, 1,
                 s->heure_ok ? "heure internet" : "heure non synchronisee");
    texte_centre(OM_WIN_X + OM_WIN_W / 2, OM_WIN_Y + OM_TITRE_H + 96, 1,
                 s->wifi ? s->ssid : "hors ligne");
}

/* ------------------------------------------------------------------ */
/* SAFARI : la page web en texte                                      */
/* ------------------------------------------------------------------ */
static void elide(char *dst, size_t cap, int s, int maxw, const char *src)
{
    size_t maxc = (size_t)(maxw / (4 * s));
    if (maxc >= cap) maxc = cap - 1;
    size_t n = src ? strlen(src) : 0;
    if (n <= maxc) { snprintf(dst, cap, "%s", src ? src : ""); return; }
    if (maxc < 4) { dst[0] = 0; return; }
    memcpy(dst, src, maxc - 2);
    dst[maxc - 2] = '.';
    dst[maxc - 1] = '.';
    dst[maxc] = 0;
}

static void vue_safari(const app_t *s)
{
    fenetre("Safari");
    int x = OM_WIN_X + 5, y = OM_WIN_Y + OM_TITRE_H + 5;
    int cx = OM_WIN_X + OM_WIN_W / 2;
    char l[64];

    if (s->saf_etat == 1) {                     /* en cours */
        texte_centre(cx, y + 38, 2, "CHARGEMENT");
        elide(l, sizeof(l), 1, OM_WIN_W - 20, safari_url());
        texte(x, y + 58, 1, l);
        texte(x, y + 70, 1, "HTTPS, TEXTE SEUL, PAS DE JAVA");
        return;
    }
    if (s->saf_etat == 3) {                     /* echec assume */
        texte_centre(cx, y + 26, 2, "ECHEC");
        elide(l, sizeof(l), 1, OM_WIN_W - 20, s->saf_err[0] ? s->saf_err : "n.d.");
        texte_centre(cx, y + 46, 1, l);
        texte(x, y + 66, 1, "B1 : REESSAYER");
        texte(x, y + 76, 1, "B2 LONG : BUREAU");
        return;
    }
    if (s->saf_etat == 0) {                     /* pas encore charge */
        texte_centre(cx, y + 26, 2, "AUCUNE PAGE");
        elide(l, sizeof(l), 1, OM_WIN_W - 20, safari_url());
        texte(x, y + 48, 1, l);
        texte(x, y + 62, 1, "B1 : CHARGER");
        return;
    }

    /* la page, 13 lignes de 37 caracteres */
    int pages = s->saf_pages > 0 ? s->saf_pages : 1;
    int p = pages > 0 ? s->page % pages : 0;
    int debut = p * SAFARI_LIGNES_VUES;
    for (int i = 0; i < SAFARI_LIGNES_VUES; i++) {
        const char *t = safari_ligne(debut + i);
        if (!t) break;
        texte(x, y + i * 9, 1, t);
    }
    snprintf(l, sizeof(l), "PAGE %d/%d  %d L  %d Ko",
             p + 1, pages, s->saf_lignes, (s->saf_octets + 512) / 1024);
    texte(x, OM_WIN_Y + OM_WIN_H - 13, 1, l);
}

/* ------------------------------------------------------------------ */
/* l'ecran complet                                                    */
/* ------------------------------------------------------------------ */
void ui_dessine(const app_t *s)
{
    art_clear();

    /* la dalle de demarrage */
    if (s->boot < 2) {
        if (s->boot == 0) {
            cadre(84, 74, 32, 40);
            texte_centre(100, 86, 2, "?");
            texte_centre(100, 130, 1, "Inserez le disque systeme");
        } else {
            /* le Happy Mac */
            rempli(78, 58, 44, 44, 1);
            rempli(81, 61, 38, 38, 0);
            a_hline(82, 117, 90, 1);
            rempli(82, 92, 35, 6, 1);
            rempli(90, 66, 3, 7, 1);
            rempli(107, 66, 3, 7, 1);
            for (int i = 0; i < 12; i++) {
                int dy = i < 6 ? i : 11 - i;
                a_px(92 + i, 78 + dy, 1);
            }
            texte_centre(100, 116, 1, "Bienvenue sur Macintosh");
            cadre(60, 126, 80, 7);
            rempli(61, 127, 50, 5, 1);
            texte_centre(100, 140, 1, "Systeme 6.0.8");
        }
        return;
    }

    /* le bureau, en trame, puis la barre de menus */
    trame(0, OM_MB, OM_W, OM_H - OM_MB);
    barre_menus(s);

    if (s->vue == VUE_BUREAU) {
        for (int i = 0; i < OM_ICONES; i++) {
            icone(i, IC_X[i], IC_Y[i], s->sel == i);
            etiquette(IC_X[i] + 10, IC_Y[i] + 21, IC_NOM[i]);
        }
        corbeille(160, 148, 0);
        etiquette(170, 169, "Corbeille");
    } else {
        switch (s->vue) {
        case VUE_FINDER:  vue_finder(s); break;
        case VUE_WRITE:   vue_write(s); break;
        case VUE_PAINT:   vue_paint(s); break;
        case VUE_TABLEAU: vue_tableau(s); break;
        case VUE_APROPOS: vue_apropos(s); break;
        case VUE_HORLOGE: vue_horloge(s); break;
        case VUE_SAFARI:  vue_safari(s); break;
        default: break;
        }
    }

    /* le pied : les gestes, toujours sous les yeux */
    rempli(0, OM_PIED_Y, OM_W, OM_H - OM_PIED_Y, 0);
    a_hline(0, OM_W - 1, OM_PIED_Y, 1);
    if (s->vue == VUE_BUREAU) {
        texte(4, OM_PIED_Y + 3, 1, "B2 : icone   B1 : ouvrir");
        texte(4, OM_PIED_Y + 10, 1, "B2 long : MacPaint");
    } else if (s->vue == VUE_SAFARI) {
        texte(4, OM_PIED_Y + 3, 1, "B2 : page   B1 : recharger");
        texte(4, OM_PIED_Y + 10, 1, "B2 long : bureau");
    } else {
        texte(4, OM_PIED_Y + 3, 1, "B2 : page   B1 : fermer");
        texte(4, OM_PIED_Y + 10, 1, "B2 long : bureau");
    }
    if (s->message[0]) {
        int w = a_text_w(1, s->message);
        rempli(OM_W / 2 - w / 2 - 3, OM_MB + 2, w + 6, 9, 1);
        a_text_col(OM_W / 2 - w / 2, OM_MB + 4, 1, s->message, 0);
    }
}

int ui_icone_sous(int x, int y)
{
    for (int i = 0; i < OM_ICONES; i++) {
        if (x >= IC_X[i] - 4 && x <= IC_X[i] + 24 &&
            y >= IC_Y[i] - 4 && y <= IC_Y[i] + 26) return i;
    }
    return -1;
}
