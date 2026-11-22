/* geste.h — la couche d'entree du BATMino : deux boutons, cinq gestes, aucun perdu.

   POURQUOI CE FICHIER
   -------------------
   Avant, la lecture des broches et l'action etaient dans la MEME tache
   (`button_task`). Or l'action peut bloquer pres d'une seconde (rafraichissement
   e-paper, attente anti-rafale) : pendant ce temps les broches n'etaient plus
   lues et l'appui etait perdu — d'ou les rustines « on ATTEND plutot que de
   perdre l'appui » et les vTaskDelay dans les gestionnaires.

   Ici, la lecture est pure : `geste_maj()` recoit le niveau des deux broches et
   l'heure, et rend les gestes. Elle ne bloque jamais, n'alloue rien, n'appelle
   rien. La tache d'echantillonnage l'appelle toutes les 10 ms et pousse le
   resultat dans une file ; la tache d'action vide la file a son rythme, meme
   lente. Un appui n'est donc jamais perdu : il attend dans la file.

   GESTES
   ------
     COURT   appui bref relache. Sur un bouton qui accepte le double, il n'est
             confirme qu'apres `fenetre_double_ms` (320 ms) : c'est le prix du
             double appui, et le seul.
     LONG    part PENDANT l'appui, a `seuil_long_ms` (600 ms), sans attendre le
             relachement. Un appui long ne produit jamais de COURT.
     DOUBLE  deuxieme appui commence dans les `fenetre_double_ms` du relachement.
             Il annule le COURT retenu : on ne fait pas les deux.
     REPEAT  le bouton reste enfonce apres le LONG : un evenement toutes les
             `periode_repeat_ms`. C'est ce qui permet de faire defiler une liste
             avec deux boutons seulement. Le routeur l'ignore si la page ne
             l'utilise pas.
     AUCUN   rien.

   Cout : ~40 octets d'etat par bouton, aucune dependance (ni FreeRTOS ni IDF).
   Teste sur PC par tests/unit/test_geste.c. */
#ifndef GESTE_H
#define GESTE_H

#include <stdint.h>
#include <stdbool.h>

#define GESTE_BTN_NB 2      /* deux boutons, et il n'y en aura pas d'autres */
#define GESTE_FILE_NB 16    /* gestes en attente avant debordement */

typedef enum {
    GESTE_AUCUN = 0,
    GESTE_COURT,
    GESTE_LONG,
    GESTE_DOUBLE,
    GESTE_REPEAT,
} geste_type_t;

typedef struct {
    uint8_t  btn;        /* 0 = bouton 1 (BOOT), 1 = bouton 2 (PWR) */
    uint8_t  type;       /* geste_type_t */
    uint16_t duree_ms;   /* duree de l'appui qui a produit ce geste */
} geste_t;

typedef struct {
    uint16_t seuil_rebond_ms;    /* 40  : en dessous, c'est du bruit */
    uint16_t seuil_long_ms;      /* 600 : le LONG part a cette duree */
    uint16_t fenetre_double_ms;  /* 320 : fenetre pour enchainer un 2e appui */
    uint16_t periode_repeat_ms;  /* 400 : cadence du REPEAT apres le LONG */
    bool     double_autorise[GESTE_BTN_NB];
} geste_cfg_t;

typedef struct {
    uint8_t  niveau_prec;     /* 1 = relache, 0 = enfonce (pull-up) */
    int64_t  t_debut;         /* debut de l'appui courant */
    int64_t  t_dernier_front; /* dernier changement de niveau (anti-rebond) */
    int64_t  t_front;         /* premiere vue du front, avant confirmation */
    int64_t  t_court;         /* heure du relachement d'un COURT en attente */
    int64_t  t_repeat;        /* dernier REPEAT emis */
    bool     long_envoye;
    bool     front_attente;   /* un front observe, pas encore confirme */
    bool     court_attente;   /* COURT retenu, on guette un DOUBLE */
    bool     double_fait;     /* appui deja transforme en DOUBLE : pas de COURT */
} geste_btn_t;

typedef struct {
    geste_cfg_t cfg;
    geste_btn_t btn[GESTE_BTN_NB];
} geste_machine_t;

/* ---- la file : ce que l'echantillonneur remplit, ce que l'action vide ---- */

typedef struct {
    geste_t  ev[GESTE_FILE_NB];
    uint8_t  tete;      /* prochain a lire */
    uint8_t  n;         /* nombre d'elements valides */
    uint32_t perdus;    /* gestes arrives file pleine (doit rester a 0 en usage reel) */
} geste_file_t;

static inline geste_cfg_t geste_cfg_defaut(void)
{
    geste_cfg_t c;
    c.seuil_rebond_ms   = 40;
    c.seuil_long_ms     = 600;
    c.fenetre_double_ms = 320;
    c.periode_repeat_ms = 400;
    c.double_autorise[0] = true;    /* bouton 1 : double appui = assistant */
    c.double_autorise[1] = true;    /* bouton 2 : double appui disponible aussi */
    return c;
}

static inline void geste_init(geste_machine_t *m, const geste_cfg_t *cfg, int64_t now_ms)
{
    m->cfg = cfg ? *cfg : geste_cfg_defaut();
    for (int i = 0; i < GESTE_BTN_NB; i++) {
        m->btn[i].niveau_prec     = 1;      /* pull-up : relache au depart */
        m->btn[i].t_debut         = now_ms;
        m->btn[i].t_dernier_front = now_ms;
        m->btn[i].t_front         = now_ms;
        m->btn[i].t_court         = 0;
        m->btn[i].t_repeat        = 0;
        m->btn[i].long_envoye     = false;
        m->btn[i].front_attente   = false;
        m->btn[i].court_attente   = false;
        m->btn[i].double_fait     = false;
    }
}

/* l'echantillonneur appelle ca a chaque front d'entree ; la duree est bornee
   pour ne pas deborder le uint16 (au dela de 60 s, on s'en fiche) */
static inline uint16_t geste_duree(int64_t d)
{
    if (d < 0) return 0;
    return (uint16_t)(d > 60000 ? 60000 : d);
}

static inline int geste_ajoute(geste_t *out, int n, int max, uint8_t btn,
                               uint8_t type, uint16_t duree)
{
    if (n >= max) return n;
    out[n].btn      = btn;
    out[n].type     = type;
    out[n].duree_ms = duree;
    return n + 1;
}

/* LE COEUR : une passe d'echantillonnage.
   niveaux[i] = 1 relache, 0 enfonce (broches en pull-up).
   Ajoute au plus `max` gestes dans `out` et rend le nombre ajoute. */
static inline int geste_maj(geste_machine_t *m, const uint8_t niveaux[GESTE_BTN_NB],
                            int64_t now_ms, geste_t *out, int max)
{
    int n = 0;
    const geste_cfg_t *c = &m->cfg;

    for (int i = 0; i < GESTE_BTN_NB; i++) {
        geste_btn_t *b = &m->btn[i];
        uint8_t niv = niveaux[i] ? 1 : 0;
        int64_t tenu = now_ms - b->t_debut;

        /* 1. nouveau front : on le prend si le rebond est passe */
        if (niv != b->niveau_prec) {
            /* l'heure du geste est celle du PREMIER passage, pas celle de la
               confirmation : sinon chaque appui perdrait la duree du rebond. */
            if (!b->front_attente) { b->front_attente = true; b->t_front = now_ms; }
            if (now_ms - b->t_dernier_front < (int64_t)c->seuil_rebond_ms)
                continue;                       /* rebond : on ne change rien */
            b->t_dernier_front = now_ms;
            b->front_attente   = false;

            if (niv == 0) {                     /* ---- debut d'appui ---- */
                b->t_debut       = b->t_front;
                b->long_envoye   = false;
                b->double_fait   = false;
                if (b->court_attente) {         /* 2e appui rapide : c'est un DOUBLE */
                    b->court_attente = false;
                    b->double_fait   = true;    /* cet appui ne donnera pas de COURT */
                    n = geste_ajoute(out, n, max, (uint8_t)i, GESTE_DOUBLE,
                                     geste_duree(b->t_front - b->t_court));
                }
            } else {                            /* ---- relachement ---- */
                if (!b->long_envoye && !b->double_fait) {
                    int64_t d = b->t_front - b->t_debut;
                    if (d >= (int64_t)c->seuil_rebond_ms) {
                        if (c->double_autorise[i]) {
                            b->court_attente = true;   /* retenu : DOUBLE possible */
                            b->t_court       = b->t_front;
                        } else {
                            n = geste_ajoute(out, n, max, (uint8_t)i, GESTE_COURT,
                                             geste_duree(d));
                        }
                    }
                }
                b->long_envoye = false;         /* pret pour l'appui suivant */
            }
            b->niveau_prec = niv;
            continue;
        }
        b->front_attente = false;               /* le niveau est revenu : front perdu */

        /* 2. pas de front : on regarde le maintien */
        if (b->niveau_prec == 0) {              /* enfonce */
            if (!b->long_envoye && tenu >= (int64_t)c->seuil_long_ms) {
                b->court_attente = false;   /* un LONG ne laisse pas un COURT derriere lui */
                b->long_envoye = true;
                b->t_repeat    = now_ms;
                n = geste_ajoute(out, n, max, (uint8_t)i, GESTE_LONG,
                                 geste_duree(tenu));
            } else if (b->long_envoye &&
                       now_ms - b->t_repeat >= (int64_t)c->periode_repeat_ms) {
                b->t_repeat = now_ms;
                n = geste_ajoute(out, n, max, (uint8_t)i, GESTE_REPEAT,
                                 geste_duree(tenu));
            }
        }
    }

    /* 3. les COURT retenus dont la fenetre de double est passee */
    for (int i = 0; i < GESTE_BTN_NB; i++) {
        geste_btn_t *b = &m->btn[i];
        if (b->court_attente &&
            now_ms - b->t_court >= (int64_t)c->fenetre_double_ms) {
            b->court_attente = false;
            n = geste_ajoute(out, n, max, (uint8_t)i, GESTE_COURT,
                             geste_duree(b->t_court - b->t_debut));
        }
    }
    return n;
}

/* ---- file ---- */

static inline void geste_file_vide(geste_file_t *f)
{
    f->tete = 0; f->n = 0; f->perdus = 0;
}

static inline bool geste_file_pousse(geste_file_t *f, const geste_t *e)
{
    if (f->n >= GESTE_FILE_NB) { f->perdus++; return false; }
    f->ev[(f->tete + f->n) % GESTE_FILE_NB] = *e;
    f->n++;
    return true;
}

static inline bool geste_file_sort(geste_file_t *f, geste_t *e)
{
    if (f->n == 0) return false;
    *e = f->ev[f->tete];
    f->tete = (uint8_t)((f->tete + 1) % GESTE_FILE_NB);
    f->n--;
    return true;
}

#endif /* GESTE_H */
