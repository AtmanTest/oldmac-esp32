/*  test_safari.cpp — le convertisseur HTML -> texte de SAFARI, sur le PC.
 *  Aucune carte : on lui donne du HTML et on regarde les lignes sorties.
 *  Lancer : tests/run_unit.sh
 */
#include "safari_txt.h"

#include <stdio.h>
#include <string.h>

static int verifs, echecs;

#define VERIF(cond, quoi) do {                                  \
    verifs++;                                                   \
    if (!(cond)) { echecs++; printf("  KO %s (ligne %d)\n", quoi, __LINE__); } \
} while (0)

static safari_txt_t *neuf(void)
{
    static safari_txt_t pool[4];
    static int i;
    safari_txt_t *t = &pool[i++ & 3];
    safari_txt_init(t);
    return t;
}

static const char *tout(safari_txt_t *t)
{
    static char b[40000];
    b[0] = 0;
    for (int i = 0; i < t->n; i++) {
        strncat(b, t->lignes[i], sizeof(b) - strlen(b) - 2);
        strncat(b, "\n", sizeof(b) - strlen(b) - 2);
    }
    return b;
}

static int contient(const char *b, const char *q) { return strstr(b, q) != NULL; }

int main(void)
{
    /* 1. une balise ne sort pas, son texte oui */
    {
        safari_txt_t *t = neuf();
        const char *h = "<html><head><title>Oodrive</title></head>"
                        "<body><h1>Bonjour</h1><p>Un paragraphe simple.</p></body></html>";
        safari_txt_html(t, h, strlen(h));
        safari_txt_fin(t);
        const char *b = tout(t);
        VERIF(contient(b, "Oodrive"), "le titre est garde");
        VERIF(contient(b, "Bonjour"), "le h1 est garde");
        VERIF(contient(b, "Un paragraphe simple."), "le paragraphe est garde");
        VERIF(!contient(b, "<"), "aucune balise ne sort");
    }

    /* 2. script et style sont jetes, commentaire aussi */
    {
        safari_txt_t *t = neuf();
        const char *h = "<p>avant</p><script>var x = 'alert(1)';</script>"
                        "<style>.a{color:red}</style><!-- cache --><p>apres</p>";
        safari_txt_html(t, h, strlen(h));
        safari_txt_fin(t);
        const char *b = tout(t);
        VERIF(contient(b, "avant"), "texte avant le script");
        VERIF(contient(b, "apres"), "texte apres le style");
        VERIF(!contient(b, "alert"), "le javascript ne sort pas");
        VERIF(!contient(b, "color"), "le css ne sort pas");
        VERIF(!contient(b, "cache"), "le commentaire ne sort pas");
    }

    /* 3. entites decodees */
    {
        safari_txt_t *t = neuf();
        const char *h = "<p>Cloud &amp; partage &lt;sur&gt; &quot;mesure&quot; "
                        "caf&eacute; 100&nbsp;% &hellip;</p>";
        safari_txt_html(t, h, strlen(h));
        safari_txt_fin(t);
        const char *b = tout(t);
        VERIF(contient(b, "Cloud & partage"), "&amp; devient &");
        VERIF(contient(b, "<sur>"), "&lt; et &gt;");
        VERIF(contient(b, "cafe"), "&eacute; plie en e");
        VERIF(contient(b, "100 %"), "&nbsp; devient une espace");
        VERIF(contient(b, "..."), "&hellip; devient ...");
    }

    /* 4. accents UTF-8 plies (la police 3x5 n'en a pas) */
    {
        safari_txt_t *t = neuf();
        const char *h = "<p>Signature electronique, donnees, cybersecurite</p>";
        /* ecrit en dur en UTF-8 : é à è ç */
        const char *u = "<p>Signature \xC3\xA9lectronique, donn\xC3\xA9""es, "
                        "s\xC3\xBB""ret\xC3\xA9, \xC3\xA7""a</p>";
        (void)h;
        safari_txt_html(t, u, strlen(u));
        safari_txt_fin(t);
        const char *b = tout(t);
        VERIF(contient(b, "Signature electronique"), "e accent aigu plie");
        VERIF(contient(b, "donnees"), "e accent aigu plie (2)");
        VERIF(contient(b, "surete"), "u accent grave plie");
        VERIF(contient(b, "ca"), "c cedille plie");
        VERIF(!contient(b, "\xC3"), "aucun octet UTF-8 ne sort");
    }

    /* 5. aucune ligne ne depasse la largeur */
    {
        safari_txt_t *t = neuf();
        char h[4000];
        strcpy(h, "<p>");
        for (int i = 0; i < 120; i++) strcat(h, "mot ");
        strcat(h, "</p>");
        safari_txt_html(t, h, strlen(h));
        safari_txt_fin(t);
        int trop = 0;
        for (int i = 0; i < t->n; i++) if ((int)strlen(t->lignes[i]) > SAFARI_COLS) trop++;
        VERIF(trop == 0, "aucune ligne de plus de 37 caracteres");
        VERIF(t->n >= 10, "les 120 mots tiennent sur plusieurs lignes");
    }

    /* 6. un mot plus long que la ligne est coupe, pas perdu */
    {
        safari_txt_t *t = neuf();
        char h[200];
        strcpy(h, "<p>");
        for (int i = 0; i < 60; i++) strcat(h, "A");
        strcat(h, " fin</p>");
        safari_txt_html(t, h, strlen(h));
        safari_txt_fin(t);
        VERIF(t->n >= 2, "le mot demesure occupe deux lignes");
        VERIF(contient(tout(t), "fin"), "la suite du texte est gardee");
    }

    /* 7. le flux par morceaux donne le meme resultat que d'un coup */
    {
        const char *h = "<p>Le partage de fichiers sensibles, "
                        "la signature electronique et la resilience.</p><p>Fin.</p>";
        int n = (int)strlen(h);
        safari_txt_t *a = neuf();
        safari_txt_html(a, h, (size_t)n);
        safari_txt_fin(a);
        static char attendu[2000];
        strcpy(attendu, tout(a));

        safari_txt_t *b = neuf();
        for (int i = 0; i < n; i += 7)
            safari_txt_html(b, h + i, (size_t)((n - i < 7) ? n - i : 7));
        safari_txt_fin(b);
        VERIF(strcmp(attendu, tout(b)) == 0, "morceaux de 7 octets = d'un seul coup");
    }

    /* 8. pagination */
    {
        safari_txt_t *t = neuf();
        const char *h = "<p>un</p><p>deux</p><p>trois</p><p>quatre</p>";
        safari_txt_html(t, h, strlen(h));
        safari_txt_fin(t);
        VERIF(safari_txt_pages(t, 13) >= 1, "au moins une page");
        safari_txt_t *v = neuf();
        VERIF(safari_txt_pages(v, 13) == 1, "page vide : 1 page");
        VERIF(safari_txt_pages(t, 1) == t->n, "une ligne par page = n pages");
    }

    /* 9. saturation : 240 lignes maximum, drapeau tronque */
    {
        safari_txt_t *t = neuf();
        char h[64];
        static char gros[300 * 40];
        gros[0] = 0;
        for (int i = 0; i < 300; i++) {
            snprintf(h, sizeof(h), "<p>ligne %03d du site</p>", i);
            strncat(gros, h, sizeof(gros) - strlen(gros) - 2);
        }
        safari_txt_html(t, gros, strlen(gros));
        safari_txt_fin(t);
        VERIF(t->n <= SAFARI_LIGNES, "jamais plus de 240 lignes");
        VERIF(t->tronque == 1, "le drapeau tronque est leve");
    }

    /* 9b. les balises en ligne ne collent pas les mots */
    {
        safari_txt_t *t = neuf();
        const char *h = "<h3>external <span>sharing</span><span>Exchange</span></h3>";
        safari_txt_html(t, h, strlen(h));
        safari_txt_fin(t);
        VERIF(contient(tout(t), "sharing Exchange"), "span ferme = une espace");
        VERIF(!contient(tout(t), "sharingExchange"), "pas de mots colles");
    }

    /* 10. un vrai morceau de la page Oodrive */
    {
        safari_txt_t *t = neuf();
        const char *h = "<div class=\"hero\"><h2>Secure and sovereign collaboration</h2>"
                        "<p>Co-edit, share, chat, video</p></div>";
        safari_txt_html(t, h, strlen(h));
        safari_txt_fin(t);
        const char *b = tout(t);
        VERIF(contient(b, "Secure and sovereign collaboration"), "titre du site conserve");
        VERIF(contient(b, "Co-edit, share, chat, video"), "sous-titre conserve");
    }

    printf("safari (html->texte)  %d verifications, %d echec(s)\n", verifs, echecs);
    return echecs ? 1 : 0;
}
