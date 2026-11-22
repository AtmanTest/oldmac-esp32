/*  safari.cpp — le cote carte de SAFARI : une tache qui tire la page en
 *  HTTPS et la confie au convertisseur HTML -> texte (safari_txt.cpp).
 *
 *  Pieges traites ici :
 *    - la pile de la tache vit en PSRAM : un handshake TLS ne tient pas
 *      dans 8 Ko de RAM interne (la carte redemarre sinon) ;
 *    - le corps se lit a la main (open / fetch_headers / read) : perform()
 *      sans gestionnaire d'evenement laisse le corps dans le socket ;
 *    - le certificat est verifie (bundle IDF), aucun mode insecure ;
 *    - l'echec est affiche (code HTTP, raison), jamais maquille en page vide.
 */
#include "safari.h"
#include "safari_txt.h"
#include "app.h"
#include "user_config.h"

#include <string.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"

static const char *TAG = "safari";

static safari_txt_t *g_txt;          /* en PSRAM : ~9,5 Ko */
static char  g_url[96] = SAFARI_URL;
static volatile bool g_demande;
static volatile int  g_etat;          /* 0 jamais, 1 en cours, 2 pret, 3 erreur */
static volatile int  g_code, g_octets, g_lignes, g_ms, g_pages_n;
static char  g_err[28];

/* ------------------------------------------------------------------ */
/* lecture pour l'ecran                                               */
/* ------------------------------------------------------------------ */
const char *safari_ligne(int i)
{
    if (!g_txt || i < 0 || i >= g_txt->n) return NULL;
    return g_txt->lignes[i];
}

int safari_nb_lignes(void) { return g_txt ? g_txt->n : 0; }

int safari_pages(void)
{
    if (!g_txt) return 1;
    return safari_txt_pages(g_txt, SAFARI_LIGNES_VUES);
}

const char *safari_url(void) { return g_url; }

int  safari_etat(void)    { return g_etat; }
int  safari_code(void)    { return g_code; }
int  safari_octets(void)  { return g_octets; }
int  safari_ms(void)      { return g_ms; }
const char *safari_erreur(void) { return g_err; }

/* ------------------------------------------------------------------ */
/* chargement                                                         */
/* ------------------------------------------------------------------ */
static void echec(const char *quoi, int code)
{
    snprintf(g_err, sizeof(g_err), "%s %d", quoi, code);
    if (code == 0) snprintf(g_err, sizeof(g_err), "%s", quoi);
    g_etat = 3;
    ESP_LOGW(TAG, "echec : %s", g_err);
    app_redessine(true);
}

static void safari_tache(void *arg)
{
    (void)arg;
    static char buf[1024];
    for (;;) {
        if (!g_demande) { vTaskDelay(pdMS_TO_TICKS(200)); continue; }
        g_demande = false;

        g_etat = 1; g_code = 0; g_octets = 0; g_ms = 0; g_err[0] = 0;
        if (g_txt) safari_txt_init(g_txt);
        app_redessine(true);

        int64_t t0 = esp_timer_get_time() / 1000;

        esp_http_client_config_t cfg = {};
        cfg.url = g_url;
        cfg.crt_bundle_attach = esp_crt_bundle_attach;
        cfg.timeout_ms = 25000;
        cfg.buffer_size = 2048;
        cfg.user_agent = "oldmac-esp32/1.1 (lecteur de texte, 200x200)";
        cfg.disable_auto_redirect = false;

        esp_http_client_handle_t c = esp_http_client_init(&cfg);
        if (!c) { echec("INIT", 0); continue; }

        esp_err_t e = esp_http_client_open(c, 0);
        if (e != ESP_OK) {
            ESP_LOGW(TAG, "open : %s", esp_err_to_name(e));
            esp_http_client_cleanup(c);
            echec("RESEAU", 0);
            continue;
        }

        esp_http_client_fetch_headers(c);
        g_code = esp_http_client_get_status_code(c);
        ESP_LOGI(TAG, "GET %s -> %d", g_url, g_code);

        if (g_code < 200 || g_code >= 400) {
            esp_http_client_close(c);
            esp_http_client_cleanup(c);
            echec("HTTP", g_code);
            continue;
        }

        int lus = 0;
        while (!g_txt->tronque) {
            int r = esp_http_client_read(c, buf, sizeof(buf));
            if (r <= 0) break;
            lus += r;
            safari_txt_html(g_txt, buf, (size_t)r);
        }
        safari_txt_fin(g_txt);

        g_octets = lus;
        g_lignes = g_txt->n;
        g_pages_n = safari_pages();
        g_ms = (int)(esp_timer_get_time() / 1000 - t0);

        esp_http_client_close(c);
        esp_http_client_cleanup(c);

        if (g_lignes <= 0) {
            echec("PAGE VIDE", g_code);
            continue;
        }
        g_etat = 2;
        ESP_LOGI(TAG, "page : %d octets lus, %d lignes, %d pages, %d ms",
                 g_octets, g_lignes, g_pages_n, g_ms);
        app_redessine(true);
    }
}

/* ------------------------------------------------------------------ */
void safari_init(void)
{
    if (!g_txt) {
        g_txt = (safari_txt_t *)heap_caps_malloc(sizeof(safari_txt_t), MALLOC_CAP_SPIRAM);
        if (!g_txt) {
            ESP_LOGW(TAG, "pas de PSRAM : lecteur indisponible");
            return;
        }
        safari_txt_init(g_txt);
    }
    if (xTaskCreateWithCaps(safari_tache, "safari", 24 * 1024, NULL, 3, NULL,
                            MALLOC_CAP_SPIRAM) != pdPASS) {
        ESP_LOGW(TAG, "tache safari : creation impossible");
    }
}

void safari_lance(const char *url)
{
    if (url && url[0]) snprintf(g_url, sizeof(g_url), "%s", url);
    if (!g_txt) return;
    g_demande = true;
    ESP_LOGI(TAG, "chargement demande : %s", g_url);
}
