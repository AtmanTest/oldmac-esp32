/*  art.h - rendu 1 bit partage entre le firmware (ESP32-S3) et l'outil hote.
 *
 *  Le meme code dessine sur le robot et dans l'apercu PNG du PC : ce qu'on
 *  voit dans l'apercu est exactement ce que l'e-paper affiche.
 *
 *  Framebuffer 200x200, 1 bit par pixel, 1 = blanc, 0 = noir (comme le BSP).
 */
#ifndef ART_H_INCLUDED
#define ART_H_INCLUDED

#include <stdint.h>
#include <stdbool.h>

#define ART_W 200
#define ART_H 200
#define ART_FB_LEN (ART_W * 25)

/* canevas */
void art_begin(uint8_t *fb, int oy);   /* oy = decalage vertical (bandeau bas) */
uint8_t *art_fb(void);
void art_clear(void);

/* inclinaison appliquee a tout ce qui suit (degres, rotation autour du centre) */
void art_tilt(float deg, int cx, int cy);
void art_no_tilt(void);

/* primitives (t = epaisseur) */
void a_px(int x, int y, int black);
void a_hline(int x0, int x1, int y, int t);
void a_vline(int x, int y0, int y1, int t);
void a_disc(int cx, int cy, int r, int black);
void a_ring(int cx, int cy, int r, int t, int black);
void a_efill(int cx, int cy, int rx, int ry, int black);
void a_ering(int cx, int cy, int rx, int ry, int t, int black);
void a_seg(int x0, int y0, int x1, int y1, int t);
void a_arc(int cx, int cy, int r, float a0, float a1, int t);
void a_arc_c(int cx, int cy, int r, float a0, float a1, int t, int black);
void a_arcf(int cx, int cy, int rx, int ry, float a0, float a1, int t);
void a_tri(int x0, int y0, int x1, int y1, int x2, int y2, int black);
/* arc epais en "camembert" plein (pour sourires ouverts) */
void a_wedge(int cx, int cy, int rx, int ry, float a0, float a1, int black);

/* texte : police 3x5, s = agrandissement. renvoie la largeur utilisee */
int  a_text(int x, int y, int s, const char *str);
int  a_text_col(int x, int y, int s, const char *str, int black);  /* 0 = texte blanc */
int  a_num(int x, int y, int s, int val, int min_d);
int  a_text_w(int s, const char *str);
int  a_num_w(int s, int val, int min_d);
int  a_text_c(int cx, int y, int s, const char *str);   /* centre sur cx */

/* ---------------------------------------------------------------- */
/* icones                                                           */
/* ---------------------------------------------------------------- */
void ic_wifi(int cx, int cy, int on);
void ic_temp(int cx, int cy);
void ic_gear(int cx, int cy, int r, int t);
void ic_heart(int cx, int cy, int s, int filled);
void ic_food(int cx, int cy);
void ic_bolt(int cx, int cy, int s);
void ic_chip(int cx, int cy);
void ic_ram(int cx, int cy);
void ic_mic(int cx, int cy, int s);
void ic_bat(int cx, int cy, int s);       /* logo chauve-souris (noir) */
void ic_bat_c(int cx, int cy, int s, int black); /* idem, couleur choisie */
void ic_speaker(int cx, int cy);
void ic_power(int cx, int cy, int r);
void ic_moon(int cx, int cy, int r);

/* ---------------------------------------------------------------- */
/* BATMino : la tete                                                */
/* ---------------------------------------------------------------- */

/* formes d'yeux */
enum {
    EYE_OPEN = 0, EYE_WIDE, EYE_NARROW, EYE_HAPPY, EYE_SAD, EYE_LINE,
    EYE_HALF, EYE_STAR, EYE_HEART, EYE_CROSS, EYE_WINK, EYE_GLOW,
    EYE_LOOK_L, EYE_LOOK_R, EYE_LOOK_UP, EYE_LOOK_DOWN, EYE_TEARY,
    EYE_ANGRY, EYE_SCARED, EYE_ROLL, EYE_TINY, EYE_COUNT
};

/* bouches */
enum {
    M_FLAT = 0, M_SMILE, M_BIG_SMILE, M_OPEN, M_GRIN, M_FROWN, M_SAD_OPEN,
    M_WAVY, M_ZIGZAG, M_O, M_SMIRK, M_TEETH, M_TONGUE, M_WHISTLE, M_POUT,
    M_SCREAM, M_ANGRY, M_SNEER, M_COUNT
};

/* sourcils/cagoule : 0 neutre, 1 fronce, 2 fronce fort, 3 haut (surprise),
   4 triste (interieur haut), 5 determine */
enum { BROW_NEUTRAL = 0, BROW_ANGRY, BROW_ANGRY_HARD, BROW_UP, BROW_SAD, BROW_DET };

/* symboles flottants */
enum {
    X_NONE = 0, X_HEART, X_STAR, X_ZZZ, X_SWEAT, X_ANGER, X_QUESTION, X_SPARK,
    X_MUSIC, X_DROP, X_SKULL, X_FIRE, X_VEIN, X_SPIRAL, X_HATCH, X_CROC, X_MOTION, X_DOTS, X_PAIN
};

/* une expression = une combinaison de traits */
typedef struct {
    const char *nom;      /* minuscules, sans accent (API/LLM) */
    uint8_t famille;      /* index de famille emotionnelle */
    int8_t  oeil_g, oeil_d;
    int8_t  sourcil, sourcil_d;
    int8_t  bouche;
    int8_t  inclinaison;  /* degres */
    uint8_t symbole, sym_x, sym_y;
    uint8_t blush;        /* 0/1 : joues */
} expr_t;

extern const expr_t EXPR[];
extern const int EXPR_N;
extern const char *const FAMILLES[];   /* noms des familles */
extern const int FAMILLE_N;
extern uint8_t FAMILLE_TAILLE[];       /* nb d'expressions par famille (art_tables_init) */

void a_symbole(int code, int x, int y);   /* symbole BD (coeur, goutte, ?...) */

/* dessine la tete seule (centre ~(100,96), sans obeissance au bandeau) */
void draw_bat_head(const expr_t *e);

/* tables : a appeler une fois (fait aussi par art_begin) */
void art_tables_init(void);
int  art_famille_id(const char *nom);
int  art_expr_id(const char *nom);       /* -1 si inconnue */
int  art_expr_de_famille(int famille, int k);

/* BATMino en grand : 5 expressions, plein ecran (lisibles de loin) */
enum { BAT5_NEUTRE = 0, BAT5_CONTENT, BAT5_FACHE, BAT5_TRISTE, BAT5_SURPRIS, BAT5_N };
extern const char *const BAT5_NOM[];
void draw_bat_grand(int expression);

/* petite tete ronde pour les listes/menus */
void ic_bat_small(int cx, int cy, int s);
/* le petit compagnon rond : c'est le visage de BATMino */
/* bouche < 0 : expression habituelle selon etat.
   bouche >= 0 : il parle (0 = ferme, 1 = grand ouvert), les yeux restent ouverts. */
void draw_kirby(int cx, int cy, int r, int ph, int etat, float bouche = -1.0f);

/* ---------------------------------------------------------------- */
/* UI facon iOS : capsules, anneaux de progression, ecran TIMER     */
/* ---------------------------------------------------------------- */
void ui_capsule(int cx, int cy, int rx, int ry, int black);         /* capsule pleine */
void ui_capsule_contour(int cx, int cy, int rx, int ry, int t);     /* capsule vide  */
void ui_anneau(int cx, int cy, int r, int pct, int t, int black);   /* anneau de progression */

/* ecran TIMER/ALARME complet (identique sur la carte et dans l'apercu) */
void ui_ecran_timer(const char *titre, const char *haut_d,
                    const char *const noms[], const char *const sous[],
                    const int pcts[], int n, int sel,
                    const char *bas1, const char *bas2);

#endif /* ART_H_INCLUDED */
