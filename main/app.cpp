/*  app.cpp — le Systeme du Macintosh miniature : demarrage, gestes,
 *  wifi, heure internet, mesures reelles, et l'API de verification.
 *
 *  Aucune image, aucune donnee inventee : ce qui s'affiche est lu sur la
 *  carte (memoire, temperature, wifi) ou vient de l'heure internet. Quand
 *  une valeur manque, l'ecran ecrit « n.d. ».
 */
#include "app.h"
#include "ui.h"
#include "safari.h"
#include "art.h"
#include "user_config.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_http_server.h"
#include "esp_system.h"
#include "driver/temperature_sensor.h"
#include "geste.h"

static const char *TAG = "oldmac";

static app_t g;
static void (*g_ecran)(const uint8_t *fb, bool plein);
static uint8_t fb[ART_FB_LEN];
static uint8_t fb_envoye[ART_FB_LEN];
static bool    envoi_valide;
static bool    a_redessiner;
static bool    plein_demande;
static int64_t boot_ms;
static int64_t mesure_ms;
static int64_t tick_ms;
static temperature_sensor_handle_t tsens;
static bool tsens_ok;
static bool sntp_lance;

static int64_t maintenant_ms(void) { return esp_timer_get_time() / 1000; }

app_t *app_etat(void) { return &g; }
bool app_plein_rafraichissement(void) { return plein_demande; }

void app_redessine(bool plein)
{
    a_redessiner = true;
    if (plein) plein_demande = true;
}

/* ------------------------------------------------------------------ */
/* heure                                                              */
/* ------------------------------------------------------------------ */
static void maj_heure(void)
{
    time_t t = time(NULL);
    if (!g.heure_ok) {
        strcpy(g.heure, "--:--");
        strcpy(g.date, "--/--");
        return;
    }
    struct tm tm;
    localtime_r(&t, &tm);
    snprintf(g.heure, sizeof(g.heure), "%02u:%02u",
             (unsigned)(tm.tm_hour % 24), (unsigned)(tm.tm_min % 60));
    snprintf(g.date, sizeof(g.date), "%02u/%02u",
             (unsigned)(tm.tm_mday % 32), (unsigned)((tm.tm_mon + 1) % 13));
}

/* ------------------------------------------------------------------ */
/* mesures reelles                                                    */
/* ------------------------------------------------------------------ */
static void maj_mesures(void)
{
    g.heap_libre = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    g.psram_libre = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    g.uptime_min = (int)(esp_timer_get_time() / 60000000LL);

    if (tsens_ok) {
        float c = 0;
        if (temperature_sensor_get_celsius(tsens, &c) == ESP_OK) g.temp_c = c;
        else g.temp_c = -100;
    } else {
        g.temp_c = -100;
    }

    /* l'etat reseau se lit sur l'interface, pas sur un drapeau : l'evenement
       IP_EVENT n'est pas toujours recu (constate sur cette carte), et une
       icone « hors ligne » alors que la page se charge serait un mensonge */
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip;
    bool up = (netif && esp_netif_get_ip_info(netif, &ip) == ESP_OK && ip.ip.addr != 0);
    if (up) {
        g.wifi = true;
        snprintf(g.ip, sizeof(g.ip), IPSTR, IP2STR(&ip.ip));
        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
            g.rssi = ap.rssi;
            snprintf(g.ssid, sizeof(g.ssid), "%.23s", (const char *)ap.ssid);
        }
        /* la page de SAFARI se charge une fois le reseau la, toute seule */
        static bool saf_auto;
        if (!saf_auto) { saf_auto = true; safari_lance(NULL); }
    } else {
        g.wifi = false;
        g.rssi = 0;
        snprintf(g.ip, sizeof(g.ip), "n.d.");
    }
    maj_heure();

    /* SAFARI : l'etat change quand la page arrive ou quand elle echoue */
    int av = g.saf_etat;
    g.saf_etat   = safari_etat();
    g.saf_code   = safari_code();
    g.saf_lignes = safari_nb_lignes();
    g.saf_pages  = safari_pages();
    g.saf_octets = safari_octets();
    g.saf_ms     = safari_ms();
    snprintf(g.saf_err, sizeof(g.saf_err), "%s", safari_erreur());
    if (av != g.saf_etat) a_redessiner = true;
}

/* ------------------------------------------------------------------ */
/* wifi                                                               */
/* ------------------------------------------------------------------ */
static const char *ssid_courant;

static void wifi_demarre(void)
{
    static bool init;
    if (!init) {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_sta();
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        init = true;
    }
    wifi_config_t wc = {};
    snprintf((char *)wc.sta.ssid, sizeof(wc.sta.ssid), "%s", ssid_courant);
    snprintf((char *)wc.sta.password, sizeof(wc.sta.password), "%s", WIFI_PASS);
    wc.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_connect();
    ESP_LOGI(TAG, "wifi : essai sur %s", ssid_courant);
}

static void wifi_scan_diagnostic(void)
{
    wifi_scan_config_t sc = {};
    if (esp_wifi_scan_start(&sc, true) != ESP_OK) {
        ESP_LOGW(TAG, "scan wifi : impossible");
        return;
    }
    uint16_t n = 0;
    esp_wifi_scan_get_ap_num(&n);
    if (n > 15) n = 15;
    wifi_ap_record_t *ap = (wifi_ap_record_t *)calloc(n ? n : 1, sizeof(wifi_ap_record_t));
    if (!ap) return;
    if (esp_wifi_scan_get_ap_records(&n, ap) == ESP_OK) {
        ESP_LOGI(TAG, "scan : %d reseaux", (int)n);
        for (int i = 0; i < (int)n; i++)
            ESP_LOGI(TAG, "  '%s' %d dBm auth=%d", (const char *)ap[i].ssid,
                     ap[i].rssi, (int)ap[i].authmode);
    }
    free(ap);
}

static void wifi_evenement(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *d = (wifi_event_sta_disconnected_t *)data;
        ESP_LOGW(TAG, "wifi : deconnecte, raison %d (ssid '%s')",
                 d ? d->reason : -1, ssid_courant);
        bool etait = g.wifi;
        g.wifi = false;
        if (etait) { app_redessine(true); }
        static int essais;
        essais++;
        if (essais >= 6) {
            essais = 0;
            g.essai_wifi = (g.essai_wifi + 1) % 3;
            ssid_courant = g.essai_wifi == 0 ? WIFI_SSID :
                           g.essai_wifi == 1 ? WIFI_SSID_ALT : WIFI_SSID_ALT2;
            ESP_LOGW(TAG, "wifi : reseau suivant (%s)", ssid_courant);
            esp_wifi_disconnect();
            wifi_demarre();
        } else {
            esp_wifi_connect();
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        g.wifi = true;
        g.essai_wifi = 0;
        ESP_LOGI(TAG, "wifi connecte sur %s", ssid_courant);
        if (!sntp_lance) {
            esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
            esp_netif_sntp_init(&cfg);
            sntp_lance = true;
        }
        app_redessine(true);
    }
}

static void wifi_init(void)
{
    g.essai_wifi = 0;
    ssid_courant = WIFI_SSID;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                        wifi_evenement, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                        wifi_evenement, NULL, NULL);
    wifi_demarre();
    vTaskDelay(pdMS_TO_TICKS(1200));
    wifi_scan_diagnostic();
}

/* ------------------------------------------------------------------ */
/* API de verification (sur le reseau local)                          */
/* ------------------------------------------------------------------ */
static void ecran_texte(char *dst, size_t n)
{
    /* vue 2x plus petite : 100 colonnes, 100 lignes, pour lire vite */
    size_t k = 0;
    for (int y = 0; y < OM_H; y += 2) {
        for (int x = 0; x < OM_W; x += 2) {
            int noir = 0;
            for (int dy = 0; dy < 2; dy++)
                for (int dx = 0; dx < 2; dx++) {
                    int px = x + dx, py = y + dy;
                    if (px < OM_W && py < OM_H) {
                        int octet = fb[py * 25 + (px >> 3)];
                        int bit = (octet >> (7 - (px & 7))) & 1;   /* 1 = blanc */
                        if (!bit) noir++;
                    }
                }
            if (k + 1 < n) dst[k++] = noir >= 2 ? '#' : (noir ? '+' : '.');
        }
        if (k + 1 < n) dst[k++] = '\n';
    }
    if (k < n) dst[k] = 0;
}

static esp_err_t h_ecran(httpd_req_t *r)
{
    static char buf[OM_W / 2 * (OM_H / 2) + OM_H / 2 + 8];
    ecran_texte(buf, sizeof(buf));
    httpd_resp_set_type(r, "text/plain; charset=utf-8");
    return httpd_resp_send(r, buf, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t h_etat(httpd_req_t *r)
{
    char b[900];
    snprintf(b, sizeof(b),
             "{\"version\":\"" OM_VERSION "\",\"vue\":%d,\"sel\":%d,\"page\":%d,"
             "\"boot\":%d,\"wifi\":%s,\"ssid\":\"%s\",\"rssi\":%d,\"ip\":\"%s\","
             "\"heure\":\"%s\",\"heure_ok\":%s,\"date\":\"%s\","
             "\"heap\":%u,\"psram\":%u,\"uptime_min\":%d,\"temp\":%.1f,"
             "\"rendu_ms\":%u,"
             "\"safari\":{\"etat\":%d,\"code\":%d,\"lignes\":%d,\"pages\":%d,"
             "\"octets\":%d,\"ms\":%d,\"url\":\"%s\",\"erreur\":\"%s\"}}",
             g.vue, g.sel, g.page, g.boot, g.wifi ? "true" : "false",
             g.ssid[0] ? g.ssid : ssid_courant, g.rssi, g.ip, g.heure,
             g.heure_ok ? "true" : "false", g.date, (unsigned)g.heap_libre,
             (unsigned)g.psram_libre, g.uptime_min, (double)g.temp_c,
             (unsigned)g.rendu_ms,
             g.saf_etat, g.saf_code, g.saf_lignes, g.saf_pages,
             g.saf_octets, g.saf_ms, safari_url(), g.saf_err);
    httpd_resp_set_type(r, "application/json");
    return httpd_resp_send(r, b, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t h_vue(httpd_req_t *r)
{
    char q[32];
    int n = 0;
    if (httpd_req_get_url_query_str(r, q, sizeof(q)) == ESP_OK) {
        char v[8];
        if (httpd_query_key_value(q, "n", v, sizeof(v)) == ESP_OK) n = atoi(v);
    }
    app_vue(n);
    return httpd_resp_sendstr(r, "ok\n");
}

static esp_err_t h_bouton(httpd_req_t *r)
{
    char q[48];
    int btn = 1, type = GESTE_COURT;
    if (httpd_req_get_url_query_str(r, q, sizeof(q)) == ESP_OK) {
        char v[8];
        if (httpd_query_key_value(q, "b", v, sizeof(v)) == ESP_OK) btn = atoi(v);
        if (httpd_query_key_value(q, "t", v, sizeof(v)) == ESP_OK) {
            if (!strcmp(v, "long")) type = GESTE_LONG;
            else if (!strcmp(v, "double")) type = GESTE_DOUBLE;
            else type = GESTE_COURT;
        }
    }
    app_geste(btn, type, 100);
    return httpd_resp_sendstr(r, "ok\n");
}

static esp_err_t h_safari(httpd_req_t *r)
{
    char q[128], u[96];
    const char *url = NULL;
    if (httpd_req_get_url_query_str(r, q, sizeof(q)) == ESP_OK) {
        char v[96];
        if (httpd_query_key_value(q, "url", v, sizeof(v)) == ESP_OK) {
            snprintf(u, sizeof(u), "%s", v);
            url = u;
        }
    }
    safari_lance(url);
    return httpd_resp_sendstr(r, "ok\n");
}

static esp_err_t h_racine(httpd_req_t *r)
{
    const char *html =
        "<!doctype html><meta charset=utf-8><title>oldmac</title>"
        "<body style='background:#111;color:#ddd;font:14px monospace'>"
        "<h1>oldmac " OM_VERSION "</h1>"
        "<p><a style='color:#6cf' href='/ecran'>/ecran</a> : l'ecran en texte<br>"
        "<a style='color:#6cf' href='/etat'>/etat</a> : l'etat en JSON<br>"
        "<a style='color:#6cf' href='/vue?n=0'>/vue?n=0..6</a> : changer de vue<br>"
        "<a style='color:#6cf' href='/bouton?b=1&t=court'>/bouton?b=1&t=court</a> : "
        "simuler un geste<br>"
        "<a style='color:#6cf' href='/safari'>/safari</a> : recharger la page "
        "web en texte<br>"
        "<a style='color:#6cf' href='/vue?n=7'>/vue?n=7</a> : ouvrir Safari"
        "</p></body>";
    httpd_resp_set_type(r, "text/html");
    return httpd_resp_send(r, html, HTTPD_RESP_USE_STRLEN);
}

static void api_demarre(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;
    cfg.lru_purge_enable = true;
    httpd_handle_t srv = NULL;
    if (httpd_start(&srv, &cfg) != ESP_OK) {
        ESP_LOGW(TAG, "serveur http : indisponible");
        return;
    }
    httpd_uri_t u1 = { "/", HTTP_GET, h_racine, NULL };
    httpd_uri_t u2 = { "/etat", HTTP_GET, h_etat, NULL };
    httpd_uri_t u3 = { "/ecran", HTTP_GET, h_ecran, NULL };
    httpd_uri_t u4 = { "/vue", HTTP_GET, h_vue, NULL };
    httpd_uri_t u5 = { "/bouton", HTTP_GET, h_bouton, NULL };
    httpd_uri_t u6 = { "/safari", HTTP_GET, h_safari, NULL };
    httpd_register_uri_handler(srv, &u1);
    httpd_register_uri_handler(srv, &u2);
    httpd_register_uri_handler(srv, &u3);
    httpd_register_uri_handler(srv, &u4);
    httpd_register_uri_handler(srv, &u5);
    httpd_register_uri_handler(srv, &u6);
    ESP_LOGI(TAG, "api : http://<ip>/ (etat, ecran, vue, bouton)");
}

/* ------------------------------------------------------------------ */
/* vues                                                               */
/* ------------------------------------------------------------------ */
void app_vue(int vue)
{
    if (vue < VUE_BUREAU || vue > VUE_SAFARI) vue = VUE_BUREAU;
    g.vue = vue;
    g.page = 0;
    g.sel = 0;
    app_redessine(true);
}

/* ------------------------------------------------------------------ */
/* gestes                                                             */
/* ------------------------------------------------------------------ */
static void message(const char *m)
{
    snprintf(g.message, sizeof(g.message), "%.39s", m);
    g.message_ms = 1400;
}

void app_geste(int btn, int type, int duree_ms)
{
    (void)duree_ms;
    if (g.boot < 2) {                  /* on saute la demonstration */
        g.boot = 2;
        app_redessine(true);
        return;
    }
    if (type == GESTE_REPEAT) type = GESTE_COURT;

    if (type == GESTE_LONG) {
        if (btn == 1) {                /* B2 tenu : le bureau, ou MacPaint */
            if (g.vue == VUE_BUREAU) { app_vue(VUE_PAINT); message("MacPaint"); }
            else { app_vue(VUE_BUREAU); message("Bureau"); }
        } else {                       /* B1 tenu : redemarrer le Mac */
            g.boot = 0;
            boot_ms = maintenant_ms();
            message("Redemarrage");
            app_redessine(true);
        }
        return;
    }

    if (type == GESTE_DOUBLE) {
        if (g.vue == VUE_BUREAU) app_vue(VUE_TABLEAU);
        else app_redessine(true);
        return;
    }

    /* appui court */
    if (btn == 1) {                    /* B2 : on deplace */
        if (g.vue == VUE_BUREAU) {
            g.sel = (g.sel + 1) % OM_ICONES;
        } else if (g.vue == VUE_FINDER) {
            g.sel = (g.sel + 1) % ui_lignes_finder();
        } else {
            g.page++;
        }
        app_redessine(false);
    } else {                           /* B1 : on decide */
        if (g.vue == VUE_BUREAU) {
            int v = ui_vue_pour_icone(g.sel);
            app_vue(v);
            message(ui_icone_nom(g.sel));
            if (v == VUE_SAFARI) safari_lance(NULL);   /* on ouvre : on charge */
        } else if (g.vue == VUE_SAFARI) {
            safari_lance(NULL);                        /* B1 : recharger */
            message("Chargement");
        } else {
            app_vue(VUE_BUREAU);
        }
    }
}

/* ------------------------------------------------------------------ */
/* boucle                                                             */
/* ------------------------------------------------------------------ */
static void dessine_et_envoie(void)
{
    int64_t t0 = maintenant_ms();
    art_begin(fb, 0);
    ui_dessine(&g);
    g.rendu_ms = (unsigned)(maintenant_ms() - t0);

    bool plein = plein_demande || !envoi_valide;
    if (!plein && memcmp(fb, fb_envoye, ART_FB_LEN) == 0) {
        a_redessiner = false;
        return;                        /* rien de neuf : on ne clignote pas */
    }
    if (g_ecran) g_ecran(fb, plein);
    memcpy(fb_envoye, fb, ART_FB_LEN);
    envoi_valide = true;
    a_redessiner = false;
    plein_demande = false;
}

void app_tick(void)
{
    int64_t now = maintenant_ms();
    if (now - tick_ms < 50) return;
    tick_ms = now;

    /* le demarrage : disquette, Happy Mac, bureau */
    if (g.boot < 2) {
        int64_t dt = now - boot_ms;
        if (g.boot == 0 && dt > 1400) { g.boot = 1; app_redessine(true); }
        else if (g.boot == 1 && dt > 3200) { g.boot = 2; app_redessine(true); }
    }

    /* les mesures et l'heure, une fois par seconde : on ne rafraichit la
       dalle que si quelque chose de visible a change (sinon l'e-paper
       clignoterait toutes les secondes pour rien) */
    if (now - mesure_ms > 1000) {
        mesure_ms = now;
        static char  v_heure[10], v_date[14], v_ssid[24], v_ip[16];
        static int   v_wifi = -1, v_rssi, v_temp, v_heap, v_uptime, v_page = -1;
        static int   v_psram, v_vue = -1;

        maj_mesures();
        int temp_dixieme = (int)(g.temp_c * 2.0f);      /* au demi-degre */
        int heap_ko = (int)(g.heap_libre / 4096);
        int psram_ko = (int)(g.psram_libre / 65536);
        bool change = strcmp(v_heure, g.heure) || strcmp(v_date, g.date) ||
                      v_wifi != (int)g.wifi || v_rssi != g.rssi ||
                      v_temp != temp_dixieme || v_heap != heap_ko ||
                      v_psram != psram_ko || v_uptime != g.uptime_min ||
                      v_page != g.page || v_vue != g.vue ||
                      strcmp(v_ssid, g.ssid) || strcmp(v_ip, g.ip);
        if (change) {
            snprintf(v_heure, sizeof(v_heure), "%s", g.heure);
            snprintf(v_date, sizeof(v_date), "%s", g.date);
            snprintf(v_ssid, sizeof(v_ssid), "%s", g.ssid);
            snprintf(v_ip, sizeof(v_ip), "%s", g.ip);
            v_wifi = g.wifi; v_rssi = g.rssi; v_temp = temp_dixieme;
            v_heap = heap_ko; v_psram = psram_ko; v_uptime = g.uptime_min;
            v_page = g.page; v_vue = g.vue;
            a_redessiner = true;
        }
    }


    if (g.message[0]) {
        g.message_ms -= 50;
        if (g.message_ms <= 0) { g.message[0] = 0; a_redessiner = true; }
    }

    if (a_redessiner) dessine_et_envoie();
}

/* ------------------------------------------------------------------ */
void app_start(const char *ssid, void (*ecran)(const uint8_t *fb, bool plein))
{
    (void)ssid;
    memset(&g, 0, sizeof(g));
    g_ecran = ecran;
    g.boot = 0;
    g.vue = VUE_BUREAU;
    g.temp_c = -100;
    g.bat_pct = -1;
    snprintf(g.heure, sizeof(g.heure), "--:--");
    snprintf(g.date, sizeof(g.date), "--/--");
    snprintf(g.ip, sizeof(g.ip), "n.d.");
    boot_ms = maintenant_ms();
    mesure_ms = 0;
    tick_ms = 0;

    temperature_sensor_config_t cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(20, 100);
    if (temperature_sensor_install(&cfg, &tsens) == ESP_OK &&
        temperature_sensor_enable(tsens) == ESP_OK) {
        tsens_ok = true;
    } else {
        ESP_LOGW(TAG, "capteur de temperature interne indisponible");
    }

    maj_mesures();
    safari_init();
    wifi_init();
    api_demarre();
    app_redessine(true);
    ESP_LOGI(TAG, "oldmac " OM_VERSION " pret (200x200, 1 bit)");
}
