/*  safari_txt.cpp — HTML -> texte, en flux, sans ESP-IDF.
 *  Voir safari_txt.h pour le contrat. Tout est deterministe et sans
 *  allocation : le meme code est teste sur le PC (tests/unit/test_safari).
 */
#include "safari_txt.h"
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* petites tables                                                     */
/* ------------------------------------------------------------------ */
static int majuscule(int c) { return (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c; }

static int est_blanc(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == 0x0b;
}

/* balises qui terminent un bloc : on pose un saut de ligne en plus */
static int est_bloc(const char *n)
{
    static const char *B[] = { "p", "br", "div", "li", "tr", "td", "th",
                               "h1", "h2", "h3", "h4", "h5", "h6",
                               "section", "article", "header", "footer",
                               "main", "nav", "ul", "ol", "table", "blockquote",
                               "figure", "figcaption", "form", "label", "button", NULL };
    for (int i = 0; B[i]; i++) if (!strcmp(n, B[i])) return 1;
    return 0;
}

/* balises dont on jette tout le contenu */
/* balises en ligne : leur fermeture vaut une espace, sinon « <span>mot
   </span><span>suite » colle les deux mots en « motsuite » */
static int est_en_ligne(const char *n)
{
    static const char *E[] = { "span", "a", "strong", "em", "b", "i", "u",
                               "small", "sup", "sub", "label", "abbr",
                               "cite", "code", "time", "mark", NULL };
    for (int i = 0; E[i]; i++) if (!strcmp(n, E[i])) return 1;
    return 0;
}

static int est_a_jeter(const char *n)
{
    return !strcmp(n, "script") || !strcmp(n, "style") || !strcmp(n, "noscript");
}

/* entites nommees les plus courantes -> ASCII */
static const char *entite(const char *e)
{
    static const struct { const char *nom, *val; } T[] = {
        { "amp", "&" },      { "lt", "<" },        { "gt", ">" },
        { "quot", "\"" },    { "apos", "'" },      { "nbsp", " " },
        { "eacute", "e" },   { "egrave", "e" },    { "ecirc", "e" },
        { "agrave", "a" },   { "acirc", "a" },     { "ccedil", "c" },
        { "ugrave", "u" },   { "ucirc", "u" },     { "ocirc", "o" },
        { "icirc", "i" },    { "iuml", "i" },      { "euml", "e" },
        { "uuml", "u" },     { "auml", "a" },      { "ouml", "o" },
        { "rsquo", "'" },    { "lsquo", "'" },     { "ldquo", "\"" },
        { "rdquo", "\"" },   { "mdash", "-" },     { "ndash", "-" },
        { "hellip", "..." }, { "laquo", "\"" },    { "raquo", "\"" },
        { "deg", "o" },      { "euro", "EUR" },    { "times", "x" },
        { "middot", "-" },   { "sect", "S" },      { "copy", "(c)" },
        { "reg", "(R)" },    { "trade", "(TM)" },  { "shy", "" },
        { NULL, NULL }
    };
    for (int i = 0; T[i].nom; i++) if (!strcmp(e, T[i].nom)) return T[i].val;
    return NULL;
}

/* ------------------------------------------------------------------ */
/* sortie                                                             */
/* ------------------------------------------------------------------ */
static void ligne_suivante(safari_txt_t *t)
{
    if (t->n < SAFARI_LIGNES) {
        t->lignes[t->n][t->col] = 0;
        t->n++;
        if (t->n < SAFARI_LIGNES) t->lignes[t->n][0] = 0;
    } else {
        t->tronque = 1;
    }
    t->col = 0;
    t->espace = 0;
}

/* place le mot en cours, en coupant la ligne si besoin */
static void pose_mot(safari_txt_t *t)
{
    int len = t->nmot;
    if (len <= 0) { t->nmot = 0; return; }
    if (t->n >= SAFARI_LIGNES) { t->tronque = 1; t->nmot = 0; return; }

    if (t->col == 0) {
        int k = len > SAFARI_COLS ? SAFARI_COLS : len;
        memcpy(t->lignes[t->n], t->mot, (size_t)k);
        t->col = k;
        t->lignes[t->n][k] = 0;
    } else if (t->col + 1 + len > SAFARI_COLS) {
        ligne_suivante(t);
        pose_mot(t);                    /* le mot repart en tete de ligne */
    } else {
        char *L = t->lignes[t->n];
        L[t->col++] = ' ';
        memcpy(L + t->col, t->mot, (size_t)len);
        t->col += len;
        L[t->col] = 0;
    }
    t->nmot = 0;
    t->espace = 0;
}

static void ajoute_car(safari_txt_t *t, char c)
{
    if (t->n >= SAFARI_LIGNES) { t->tronque = 1; return; }
    if (t->nmot >= SAFARI_MOT) pose_mot(t);      /* mot demesure : on coupe */
    t->mot[t->nmot++] = c;
}

static void ajoute_texte(safari_txt_t *t, const char *s)
{
    for (const char *p = s; *p; p++) {
        if (est_blanc(*p)) pose_mot(t), t->espace = 1;
        else               ajoute_car(t, *p);
    }
}

/* fin de bloc : un saut, et une ligne vide seulement si la derniere
   ligne ecrite n'est pas deja vide (pas de pages de blancs) */
static void fin_bloc(safari_txt_t *t, int vide)
{
    pose_mot(t);
    if (t->col > 0) { ligne_suivante(t); if (!vide) return; }
    if (vide && t->n > 0 && t->lignes[t->n - 1][0] != 0 && t->n < SAFARI_LIGNES) {
        ligne_suivante(t);
    }
}

/* ------------------------------------------------------------------ */
/* UTF-8 : pliage des accents (la police 3x5 n'en a aucun)            */
/* ------------------------------------------------------------------ */
static const char *plie_utf8(const unsigned char *s, int dispo)
{
    if (dispo < 2) return NULL;
    unsigned char a = s[0], b = s[1];
    if (a == 0xC2) {
        if (b == 0xA0) return " ";
        if (b == 0xAB || b == 0xBB) return "\"";
        if (b == 0xB0) return "o";
        if (b == 0xA9) return "(c)";
        return NULL;
    }
    if (a == 0xC3) {
        if (b >= 0xA0 && b <= 0xA5) return "a";
        if (b == 0xA6 || b == 0xA7) return "c";
        if (b >= 0xA8 && b <= 0xAB) return "e";
        if (b >= 0xAC && b <= 0xAF) return "i";
        if (b == 0xB1) return "n";
        if (b >= 0xB2 && b <= 0xB6) return "o";
        if (b == 0xB8) return "o";
        if (b >= 0xB9 && b <= 0xBC) return "u";
        if (b == 0xBF) return "y";
        return NULL;
    }
    if (a == 0xE2 && dispo >= 3) {
        unsigned char c = s[2];
        if (b == 0x80) {
            if (c == 0x98 || c == 0x99) return "'";
            if (c == 0x9C || c == 0x9D) return "\"";
            if (c == 0x93 || c == 0x94) return "-";
            if (c == 0xA6) return "...";
            if (c == 0xAF) return "x";
        }
        return NULL;
    }
    return NULL;
}

static int utf8_len(unsigned char c)
{
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

/* ------------------------------------------------------------------ */
/* machine a etats                                                    */
/* ------------------------------------------------------------------ */
enum { TXT = 0, BALISE = 1, COMMENTAIRE = 2, ENTITE = 3, ENTITE_DIESE = 4 };

void safari_txt_init(safari_txt_t *t)
{
    memset(t, 0, sizeof(*t));
    t->etat = TXT;
    if (SAFARI_LIGNES > 0) t->lignes[0][0] = 0;
}

static void fin_balise(safari_txt_t *t)
{
    if (t->nnom >= 0) t->nom[t->nnom] = 0;   /* -1 : nom deja termine */
    if (t->nom[0] == 0) { t->etat = TXT; t->nnom = 0; return; }
    if (t->saute) {
        /* on ne sort du script/style que sur SA balise fermante et seulement
           si c'en est vraiment une : le javascript contient des « < » et des
           chaines comme "<script src=...>" qui ne doivent pas rouvrir le texte */
        if (t->balise_fin && !strcmp(t->nom, t->saute_nom)) t->saute = 0;
    } else if (est_a_jeter(t->nom)) {
        t->saute = 1;
        snprintf(t->saute_nom, sizeof(t->saute_nom), "%s", t->nom);
    } else if (!strcmp(t->nom, "br")) {
        fin_bloc(t, 0);
    } else if (est_bloc(t->nom)) {
        fin_bloc(t, 1);
    } else if (t->balise_fin && est_en_ligne(t->nom)) {
        pose_mot(t);
        t->espace = 1;
    }

    t->etat = TXT;
    t->nnom = 0;
}

static void fin_entite(safari_txt_t *t)
{
    t->ent[t->nent] = 0;
    const char *v = entite(t->ent);
    if (v) ajoute_texte(t, v);
    t->etat = TXT;
    t->nent = 0;
}

int safari_txt_html(safari_txt_t *t, const char *s, size_t n)
{
    const unsigned char *p = (const unsigned char *)s;
    for (size_t i = 0; i < n && !t->tronque; i++) {
        unsigned char c = p[i];

        if (t->etat == COMMENTAIRE) {
            if (c == '-' && i + 2 < n && p[i + 1] == '-' && p[i + 2] == '>') {
                t->etat = TXT;
                i += 2;
            }
            continue;
        }

        if (t->etat == ENTITE || t->etat == ENTITE_DIESE) {
            if (c == ';') { fin_entite(t); continue; }
            if (t->etat == ENTITE_DIESE) {          /* &#233; : on ne decode pas */
                if (t->nent < (int)sizeof(t->ent) - 1) t->ent[t->nent++] = (char)c;
                if (t->nent >= 8) { fin_entite(t); }
                continue;
            }
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
                if (t->nent < (int)sizeof(t->ent) - 1) t->ent[t->nent++] = (char)majuscule((char)c);
                else t->etat = TXT;                 /* entite trop longue */
                continue;
            }
            t->etat = TXT;                          /* pas une entite */
            /* on retombe sur le caractere courant */
        }

        if (t->etat == BALISE) {
            if (c == '>') { fin_balise(t); continue; }
            /* le nom de la balise s'arrete au premier blanc ou au '/' ;
               un '/' en tete marque une balise fermante */
            if (c == '/') {
                if (t->nnom == 0) { t->balise_fin = 1; continue; }
                if (t->nnom > 0) { t->nom[t->nnom] = 0; t->nnom = -1; }
                continue;
            }
            if (est_blanc((char)c)) {
                if (t->nnom > 0) { t->nom[t->nnom] = 0; t->nnom = -1; }
                continue;
            }
            if (t->nnom < 0) continue;           /* attributs : on ignore */
            if (c == '=') continue;
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
                if (t->nnom < (int)sizeof(t->nom) - 1) t->nom[t->nnom++] = (char)majuscule((char)c);
                continue;
            }
            continue;                               /* guillemets, attributs */
        }

        if (c == '<') {
            if (i + 3 < n && p[i + 1] == '!' && p[i + 2] == '-' && p[i + 3] == '-') {
                t->etat = COMMENTAIRE;
                i += 3;
                continue;
            }
            t->etat = BALISE;
            t->nnom = 0;
            t->balise_fin = 0;
            continue;
        }

        if (t->saute) continue;                     /* dedans script/style */

        if (c == '&') {
            t->etat = ENTITE;
            t->nent = 0;
            if (i + 1 < n && p[i + 1] == '#') { t->etat = ENTITE_DIESE; i++; }
            continue;
        }

        if (c >= 0x80) {                            /* UTF-8 */
            int l = utf8_len(c);
            if ((int)(n - i) < l) break;            /* morceau coupe : on s'arrete la */
            const char *r = plie_utf8(p + i, l);
            if (r) ajoute_texte(t, r);
            else ajoute_car(t, '?');
            i += (size_t)(l - 1);
            continue;
        }

        if (est_blanc((char)c)) { pose_mot(t); t->espace = 1; continue; }

        ajoute_car(t, (char)c);
        t->octets++;
    }
    return t->n;
}

void safari_txt_fin(safari_txt_t *t)
{
    pose_mot(t);
    if (t->col > 0) ligne_suivante(t);
    while (t->n > 0 && t->lignes[t->n - 1][0] == 0) t->n--;
    if (t->n < SAFARI_LIGNES) t->lignes[t->n][0] = 0;
}

int safari_txt_pages(const safari_txt_t *t, int lignes_par_page)
{
    if (lignes_par_page <= 0) return 1;
    int p = (t->n + lignes_par_page - 1) / lignes_par_page;
    return p < 1 ? 1 : p;
}
