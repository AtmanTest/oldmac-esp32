/*  test_souris.cpp — la geometrie du clic, sur le PC.
 *  Les zones cliquables doivent tomber exactement sur le dessin : on teste
 *  le centre de chaque icone, la corbeille, et le vide.
 *  Lancer : tests/run_unit.sh
 */
#include "app.h"
#include "ui.h"

#include <stdio.h>

static int verifs, echecs;
#define VERIF(cond, quoi) do {                                  \
    verifs++;                                                   \
    if (!(cond)) { echecs++; printf("  KO %s (ligne %d)\n", quoi, __LINE__); } \
} while (0)

int main(void)
{
    /* 1. chaque icone se trouve a son centre (x+10, y+8) */
    for (int i = 0; i < OM_ICONES; i++) {
        int cx = ui_icone_x(i) + 10, cy = ui_icone_y(i) + 8;
        VERIF(ui_icone_sous(cx, cy) == i, "centre de l'icone");
        VERIF(ui_clic_bureau(cx, cy) == i, "clic sur l'icone");
    }

    /* 2. Safari est la 7e icone, a droite du bureau */
    VERIF(OM_ICONES == 7, "sept icones");
    VERIF(ui_vue_pour_icone(6) == VUE_SAFARI, "la 7e ouvre Safari");
    VERIF(ui_icone_x(6) > 150, "Safari est a droite");

    /* 3. la corbeille repond 99, le vide repond -1 */
    VERIF(ui_clic_bureau(170, 155) == 99, "la corbeille est cliquable");
    VERIF(ui_clic_bureau(60, 150) == -1, "le vide ne fait rien");
    VERIF(ui_clic_bureau(0, 0) == -1, "le coin de la barre de menus ne fait rien");

    /* 4. aucune zone cliquable ne sort de la dalle */
    for (int i = 0; i < OM_ICONES; i++) {
        VERIF(ui_icone_x(i) >= 0 && ui_icone_x(i) + 20 <= OM_W, "icone dans la largeur");
        VERIF(ui_icone_y(i) >= OM_MB && ui_icone_y(i) + 26 <= OM_PIED_Y, "icone au-dessus du pied");
    }

    /* 5. deux icones ne se chevauchent pas (sinon le clic est ambigu) */
    for (int i = 0; i < OM_ICONES; i++)
        for (int j = i + 1; j < OM_ICONES; j++) {
            int dx = ui_icone_x(i) - ui_icone_x(j), dy = ui_icone_y(i) - ui_icone_y(j);
            VERIF(dx * dx + dy * dy > 24 * 24, "deux icones ne se recouvrent pas");
        }

    printf("souris (geometrie du clic)  %d verifications, %d echec(s)\n", verifs, echecs);
    return echecs ? 1 : 0;
}
