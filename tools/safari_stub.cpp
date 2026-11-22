/*  safari_stub.cpp — de quoi dessiner SAFARI sur le PC, sans carte.
 *
 *  ui.cpp appelle safari_ligne() ; sur l'hote cette fonction lit un fichier
 *  de texte deja coupe a 37 colonnes (build/safari.txt), ou un exemple.
 *  Le vrai lecteur HTTPS est main/safari.cpp, qui ne compile pas ici.
 */
#include "safari.h"
#include "safari_txt.h"

#include <stdio.h>
#include <string.h>

#define MAXL 240
static char  lignes[MAXL][SAFARI_COLS + 2];
static int   n;
static int   charge;

static void charger(void)
{
    if (charge) return;
    charge = 1;
    FILE *f = fopen("build/safari.txt", "r");
    if (!f) {
        static const char *ex[] = {
            "Cloud solutions for sharing, backup",
            "and electronic signature - Oodrive",
            "",
            "Solutions",
            "",
            "Secure and sovereign collaboration",
            "",
            "Secure collaborative platform",
            "Co-edit, share, chat, video",
            "",
            "Secure internal and external sharing",
            "Exchange sensitive files",
            "",
        };
        for (unsigned i = 0; i < sizeof(ex) / sizeof(ex[0]) && n < MAXL; i++)
            snprintf(lignes[n++], sizeof(lignes[0]), "%s", ex[i]);
        return;
    }
    char b[256];
    while (n < MAXL && fgets(b, sizeof(b), f)) {
        size_t k = strlen(b);
        while (k && (b[k - 1] == '\n' || b[k - 1] == '\r')) b[--k] = 0;
        if (k > SAFARI_COLS) k = SAFARI_COLS, b[k] = 0;
        snprintf(lignes[n++], sizeof(lignes[0]), "%s", b);
    }
    fclose(f);
}

const char *safari_ligne(int i) { charger(); return (i >= 0 && i < n) ? lignes[i] : NULL; }
int safari_nb_lignes(void)      { charger(); return n; }
int safari_pages(void)          { charger(); int p = (n + SAFARI_LIGNES_VUES - 1) / SAFARI_LIGNES_VUES; return p < 1 ? 1 : p; }
const char *safari_url(void)    { return "https://www.oodrive.com/"; }
