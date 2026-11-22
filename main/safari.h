/*  safari.h — SAFARI, le lecteur de pages en mode texte.
 *
 *  La carte n'a ni navigateur, ni javascript, ni java : ce module tire une
 *  page par HTTPS, en jette les balises et garde le TEXTE, qu'il affiche
 *  dans une fenetre du Macintosh. C'est un lecteur, pas un navigateur, et
 *  l'ecran le dit.
 *
 *  Ce fichier ne depend pas d'ESP-IDF : ui.cpp peut l'inclure (et le PC
 *  peut le simuler avec tools/apercu_safari.cpp).
 */
#ifndef SAFARI_H
#define SAFARI_H

#include <stdbool.h>

#define SAFARI_LIGNES_VUES 13       /* lignes visibles dans la fenetre */

#ifdef __cplusplus
extern "C" {
#endif

/* ligne de texte de la page chargee (NULL si hors bornes) */
const char *safari_ligne(int i);
int  safari_nb_lignes(void);
int  safari_pages(void);            /* pages de 13 lignes, au moins 1 */

/* URL par defaut et URL courante (pour l'affichage) */
const char *safari_url(void);

/* cote carte : demarre la tache et demande un chargement (url NULL = defaut) */
void safari_init(void);
void safari_lance(const char *url);
int  safari_etat(void);        /* 0 jamais, 1 en cours, 2 pret, 3 erreur */
int  safari_code(void);        /* code HTTP de la derniere reponse          */
int  safari_octets(void);      /* octets HTML lus                          */
int  safari_ms(void);          /* duree du dernier chargement              */
const char *safari_erreur(void);

#ifdef __cplusplus
}
#endif

#endif /* SAFARI_H */
