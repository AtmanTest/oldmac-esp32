/*  safari_txt.h — le convertisseur HTML -> texte de SAFARI, sans ESP-IDF.
 *
 *  Le meme code tourne sur la carte et sur le PC (tests/unit/test_safari).
 *  Il recoit le HTML par morceaux (flux) et remplit des lignes de texte
 *  deja coupees a la largeur de la fenetre : la dalle n'a que 37 colonnes
 *  en corps 1, inutile de garder les balises.
 *
 *  Ce qu'il fait :
 *    - jette <script>, <style>, <noscript>, les commentaires, les balises ;
 *    - decode les entites (&amp; &eacute; &#39; ...) et plie les accents
 *      (la police 3x5 n'a pas de diacritiques) ;
 *    - coupe les lignes sur les espaces, jamais au milieu d'un mot
 *      (sauf un mot plus long que la ligne : il est coupe) ;
 *    - marque les fins de bloc (<p>, <div>, <br>, <li>...) par un saut ;
 *    - s'arrete net quand les 240 lignes sont remplies (page de 768 Ko).
 */
#ifndef SAFARI_TXT_H
#define SAFARI_TXT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define SAFARI_COLS      37     /* caracteres par ligne (150 px / 4 px) */
#define SAFARI_LIGNES   240     /* lignes gardees (13 par page a l'ecran) */
#define SAFARI_MOT       48     /* plus long mot accepte */

typedef struct {
    char   lignes[SAFARI_LIGNES][SAFARI_COLS + 1];
    int    n;                   /* lignes ecrites                        */
    int    col;                 /* colonne courante de la ligne en cours */
    int    espace;              /* un espace attend d'etre place         */
    int    bloc;                /* fin de bloc en attente                */

    /* machine a etats */
    int    etat;                /* 0 texte, 1 balise, 2 fin de balise    */
    int    commentaire;         /* on est dans un <!-- ... -->           */
    int    saute;               /* 1 : contenu de script/style a jeter   */
    char   saute_nom[16];       /* balise dont on saute le contenu       */
    int    balise_fin;          /* 1 : la balise en cours est une fermante */
    char   nom[16];             /* nom de la balise en cours             */
    int    nnom;
    char   ent[12];             /* entite en cours de lecture            */
    int    nent;
    char   mot[SAFARI_MOT + 2]; /* mot en cours                          */
    int    nmot;

    int    tronque;             /* 1 : les 240 lignes sont pleines       */
    int    octets;              /* octets HTML lus (mesure)              */
} safari_txt_t;

void safari_txt_init(safari_txt_t *t);

/* avale un morceau de HTML ; rend le nombre de lignes ecrites */
int  safari_txt_html(safari_txt_t *t, const char *s, size_t n);

/* termine : pose le dernier mot et supprime les lignes vides en trop */
void safari_txt_fin(safari_txt_t *t);

/* nombre de pages pour N lignes visibles */
int  safari_txt_pages(const safari_txt_t *t, int lignes_par_page);

#endif /* SAFARI_TXT_H */
