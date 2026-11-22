/*  main.cpp — la carte, et rien d'autre.
 *
 *  oldmac-esp32 : un Macintosh de 1986 simule sur la carte a e-paper
 *  (ESP32-S3, dalle 200x200 en 1 bit, deux boutons).
 *
 *  Ici : alimentation, e-paper, boutons, mesures. Tout le Systeme est
 *  dans app.cpp, tout le dessin dans ui.cpp, et les deux compilent aussi
 *  sur le PC pour les verifications.
 *
 *  Boutons : GPIO0 = bouton 1 (B1, decide), GPIO18 = bouton 2 (B2, deplace).
 */
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_pm.h"
#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"
#include "nvs_flash.h"

#include "board_power_bsp.h"
#include "epaper_driver_bsp.h"
#include "user_config.h"

#include "art.h"
#include "geste.h"
#include "app.h"

static const char *TAG = "oldmac";

static epaper_driver_display *epd = NULL;
static SemaphoreHandle_t epd_mux;
static uint8_t fb_envoye[ART_FB_LEN];
static bool fb_valide = false;
static int partiels = 0;

static board_power_bsp_t board_div(EPD_PWR_PIN, Audio_PWR_PIN, VBAT_PWR_PIN);

/* ------------------------------------------------------------------ */
/* console USB (pas de port serie separe sur cette carte)             */
/* ------------------------------------------------------------------ */
static int jtag_log(const char *fmt, va_list ap)
{
    char buf[512];
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    if (n > 0)
        usb_serial_jtag_write_bytes(buf, (n > (int)sizeof(buf) - 1) ? sizeof(buf) - 1 : n,
                                    pdMS_TO_TICKS(200));
    return n;
}

static void serial_start(void)
{
    usb_serial_jtag_driver_config_t cfg = {};
    cfg.rx_buffer_size = 1024;
    cfg.tx_buffer_size = 4096;
    if (usb_serial_jtag_driver_install(&cfg) != ESP_OK) return;
    esp_log_set_vprintf(jtag_log);
}

/* ------------------------------------------------------------------ */
/* e-paper : plein quand la vue change, partiel pour le curseur       */
/* ------------------------------------------------------------------ */
static void ecran_envoie(const uint8_t *image, bool plein)
{
    if (!epd || !epd_mux) return;
    if (xSemaphoreTake(epd_mux, pdMS_TO_TICKS(8000)) != pdTRUE) return;

    if (epd->panne) {                       /* dalle bloquee : on la relance */
        gpio_set_level((gpio_num_t)EPD_PWR_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(2500));
        gpio_set_level((gpio_num_t)EPD_PWR_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(400));
        epd->EPD_Panne_Reset();
        epd->EPD_Init();
        if (epd->panne) { xSemaphoreGive(epd_mux); return; }
        epd->EPD_Clear();
        epd->EPD_Display();
        epd->EPD_Init_Partial();
        epd->EPD_ShowBase(image, ART_FB_LEN);
        plein = true;
        fb_valide = false;
        partiels = 0;
    }

    if (fb_valide && !plein && memcmp(image, fb_envoye, ART_FB_LEN) == 0) {
        xSemaphoreGive(epd_mux);
        return;
    }
    if (plein || partiels >= 30) {
        epd->EPD_ShowFullClean(image, ART_FB_LEN);
        partiels = 0;
    } else {
        epd->EPD_ShowPart(image, ART_FB_LEN);
        partiels++;
    }
    memcpy(fb_envoye, image, ART_FB_LEN);
    fb_valide = true;
    xSemaphoreGive(epd_mux);
}

/* ------------------------------------------------------------------ */
/* boutons : echantillonnage pur (geste.h) puis file                   */
/* ------------------------------------------------------------------ */
static geste_machine_t g_geste;
static QueueHandle_t g_file;

static void boutons_tache(void *arg)
{
    (void)arg;
    gpio_config_t io = {};
    io.pin_bit_mask = (1ULL << BOOT_BUTTON_PIN) | (1ULL << PWR_BUTTON_PIN);
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io);

    const gpio_num_t pins[2] = { (gpio_num_t)BOOT_BUTTON_PIN, (gpio_num_t)PWR_BUTTON_PIN };
    geste_init(&g_geste, NULL, esp_timer_get_time() / 1000);

    geste_t ev[4];
    uint8_t niveaux[2] = { 1, 1 };
    for (;;) {
        for (int i = 0; i < 2; i++)
            niveaux[i] = (gpio_get_level(pins[i]) == 0) ? 0 : 1;
        int n = geste_maj(&g_geste, niveaux, esp_timer_get_time() / 1000, ev, 4);
        for (int k = 0; k < n; k++) {
            geste_t e = ev[k];
            if (xQueueSend(g_file, &e, 0) != pdTRUE) { }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* la tache du Systeme : elle vide les gestes et fait avancer la machine */
static void systeme_tache(void *arg)
{
    (void)arg;
    geste_t e;
    for (;;) {
        while (xQueueReceive(g_file, &e, 0) == pdTRUE) {
            app_geste(e.btn, e.type, e.duree_ms);
        }
        app_tick();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/* ------------------------------------------------------------------ */
extern "C" void app_main(void)
{
    esp_pm_config_t pm = {};
    pm.max_freq_mhz = 160;
    pm.min_freq_mhz = 80;
    pm.light_sleep_enable = true;
    esp_pm_configure(&pm);

    esp_err_t r = nvs_flash_init();
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    serial_start();
    esp_log_level_set("wifi", ESP_LOG_WARN);
    ESP_LOGI(TAG, "oldmac-esp32 : demarrage");

    epd_mux = xSemaphoreCreateMutex();

    /* tous les rails AVANT l'I2C et la dalle */
    board_div.POWEER_EPD_ON();
    board_div.POWEER_Audio_ON();
    board_div.VBAT_POWER_ON();
    vTaskDelay(pdMS_TO_TICKS(150));

    custom_lcd_spi_t dcfg = {};
    dcfg.cs = EPD_CS_PIN;
    dcfg.dc = EPD_DC_PIN;
    dcfg.rst = EPD_RST_PIN;
    dcfg.busy = EPD_BUSY_PIN;
    dcfg.mosi = EPD_MOSI_PIN;
    dcfg.scl = EPD_SCK_PIN;
    dcfg.spi_host = EPD_SPI_NUM;
    dcfg.buffer_len = 5000;
    epd = new epaper_driver_display(EPD_WIDTH, EPD_HEIGHT, dcfg);
    epd->EPD_Init();
    epd->EPD_Clear();
    epd->EPD_Display();
    epd->EPD_Init_Partial();
    {
        static uint8_t blanc[ART_FB_LEN];
        memset(blanc, 0xFF, sizeof(blanc));
        epd->EPD_ShowBase(blanc, ART_FB_LEN);
    }
    ESP_LOGI(TAG, "e-paper pret (200x200)");

    g_file = xQueueCreate(16, sizeof(geste_t));
    app_start(WIFI_SSID, ecran_envoie);
    xTaskCreate(boutons_tache, "boutons", 3072, NULL, 6, NULL);
    xTaskCreate(systeme_tache, "systeme", 8192, NULL, 5, NULL);

    for (;;) vTaskDelay(pdMS_TO_TICKS(10000));
}
