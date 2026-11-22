/*  ui.h — le Systeme en 1 bit, dessine avec art.h (200 x 200, 1 = blanc).
 *  Le meme dessin sert a l'ecran et a la verification texte (api /ecran).
 */
#ifndef UI_H
#define UI_H

#include <stdint.h>
#include "app.h"

/* geometrie, en pixels de la dalle */
#define OM_W 200
#define OM_H 200
#define OM_MB 13            /* barre de menus */
#define OM_WIN_X 26
#define OM_WIN_Y 18
#define OM_WIN_W 166
#define OM_WIN_H 156
#define OM_TITRE_H 11
#define OM_PIED_Y 182

void ui_dessine(const app_t *s);
int  ui_icone_x(int i);
int  ui_icone_y(int i);
const char *ui_icone_nom(int i);
int  ui_vue_pour_icone(int i);
int  ui_icone_sous(int x, int y);       /* icone touchee, -1 sinon */
int  ui_clic_bureau(int x, int y);      /* icone (ou corbeille=99), -1 sinon */
int  ui_lignes_finder(void);

#endif
