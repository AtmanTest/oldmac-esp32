/*  apercu.cpp — le meme dessin que la carte, mais sur le PC.
 *
 *  Compile ui.cpp + art.cpp sans ESP-IDF : ce qu'on voit ici est
 *  exactement ce que la dalle affichera. Sortie PBM (1 bit), convertible
 *  en PNG :  ./apercu bureau > bureau.pbm
 *
 *  Verifications : chaque vue est aussi relue en texte (100x100) pour
 *  etre comparee a ce qu'on attend.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "app.h"
#include "ui.h"
#include "safari.h"
#include "art.h"

static uint8_t fb[ART_FB_LEN];

static int bit(int x, int y)
{
    if (x < 0 || y < 0 || x >= OM_W || y >= OM_H) return 1;
    return (fb[y * 25 + (x >> 3)] >> (7 - (x & 7))) & 1;   /* 1 = blanc */
}

static void pbm(void)
{
    printf("P1\n%d %d\n", OM_W, OM_H);
    for (int y = 0; y < OM_H; y++) {
        for (int x = 0; x < OM_W; x++) putchar(bit(x, y) ? '0' : '1');
        putchar('\n');
    }
}

static void texte_ecran(void)
{
    for (int y = 0; y < OM_H; y += 2) {
        for (int x = 0; x < OM_W; x += 2) {
            int n = 0;
            for (int dy = 0; dy < 2; dy++)
                for (int dx = 0; dx < 2; dx++) n += !bit(x + dx, y + dy);
            putchar(n >= 2 ? '#' : (n ? '+' : '.'));
        }
        putchar('\n');
    }
}

int main(int argc, char **argv)
{
    const char *vue = argc > 1 ? argv[1] : "bureau";
    const char *mode = argc > 2 ? argv[2] : "pbm";

    app_t s;
    memset(&s, 0, sizeof(s));
    s.boot = 2;
    s.wifi = true;
    s.heure_ok = true;
    strcpy(s.heure, "14:32");
    strcpy(s.date, "20/11");
    snprintf(s.ssid, sizeof(s.ssid), "Bbox-AE58B901");
    strcpy(s.ip, "192.168.1.42");
    s.rssi = -54;
    s.heap_libre = 182000;
    s.psram_libre = 6100000;
    s.uptime_min = 42;
    s.temp_c = 43.5f;
    s.sel = 0;
    s.page = 0;

    if (!strcmp(vue, "bureau")) s.vue = VUE_BUREAU;
    else if (!strcmp(vue, "finder")) { s.vue = VUE_FINDER; s.sel = 2; }
    else if (!strcmp(vue, "write")) s.vue = VUE_WRITE;
    else if (!strcmp(vue, "write2")) { s.vue = VUE_WRITE; s.page = 1; }
    else if (!strcmp(vue, "paint")) s.vue = VUE_PAINT;
    else if (!strcmp(vue, "os")) { s.vue = VUE_PAINT; s.page = 1; }
    else if (!strcmp(vue, "tableau")) s.vue = VUE_TABLEAU;
    else if (!strcmp(vue, "apropos")) s.vue = VUE_APROPOS;
    else if (!strcmp(vue, "horloge")) s.vue = VUE_HORLOGE;
    else if (!strcmp(vue, "safari")) {
        s.vue = VUE_SAFARI;
        s.saf_etat = 2;
        s.saf_code = 200;
        s.saf_lignes = safari_nb_lignes();
        s.saf_pages = safari_pages();
        s.saf_octets = 768 * 1024;
        s.saf_ms = 2140;
    }
    else if (!strcmp(vue, "safari-attente")) { s.vue = VUE_SAFARI; s.saf_etat = 1; }
    else if (!strcmp(vue, "safari-echec")) {
        s.vue = VUE_SAFARI; s.saf_etat = 3;
        snprintf(s.saf_err, sizeof(s.saf_err), "RESEAU 0");
    }
    else if (!strcmp(vue, "boot")) { s.boot = 0; }
    else if (!strcmp(vue, "happy")) { s.boot = 1; }
    else if (!strcmp(vue, "message")) { s.vue = VUE_BUREAU; strcpy(s.message, "Bureau"); }
    else { fprintf(stderr, "vue inconnue : %s\n", vue); return 2; }

    art_begin(fb, 0);
    ui_dessine(&s);

    if (!strcmp(mode, "texte")) texte_ecran();
    else pbm();
    return 0;
}
