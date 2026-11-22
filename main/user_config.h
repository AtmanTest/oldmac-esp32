#ifndef USER_CONFIG_H
#define USER_CONFIG_H

#define EPD_SPI_NUM        SPI2_HOST
#define ESP32_I2C_DEV_NUM  I2C_NUM_0

#define EPD_WIDTH  200
#define EPD_HEIGHT 200

/*EPD port Init*/
#define EPD_DC_PIN    GPIO_NUM_10
#define EPD_CS_PIN    GPIO_NUM_11
#define EPD_SCK_PIN   GPIO_NUM_12
#define EPD_MOSI_PIN  GPIO_NUM_13
#define EPD_RST_PIN   GPIO_NUM_9
#define EPD_BUSY_PIN  GPIO_NUM_8
#define EPD_TP_RST_PIN GPIO_NUM_7
#define EPD_TP_INT_PIN GPIO_NUM_21

/*DEV POWER init*/
#define EPD_PWR_PIN     GPIO_NUM_6
#define Audio_PWR_PIN   GPIO_NUM_42
#define VBAT_PWR_PIN    GPIO_NUM_17

#define BOOT_BUTTON_PIN GPIO_NUM_0
#define PWR_BUTTON_PIN  GPIO_NUM_18

#define LED_PIN         GPIO_NUM_3

/*Low-power wake-up*/
#define ext_wakeup_pin_1 GPIO_NUM_0

/*ESP32 I2C Init*/
#define ESP32_I2C_SDA_PIN GPIO_NUM_47
#define ESP32_I2C_SCL_PIN GPIO_NUM_48

/*GPIO libres exposés sur le header d'extension*/
#define EXP_GPIO_1 GPIO_NUM_1
#define EXP_GPIO_2 GPIO_NUM_2

/*carte micro SD (SDMMC 1 bit)*/
#define SD_CLK_PIN          GPIO_NUM_39
#define SD_MOSI_CMD_PIN     GPIO_NUM_41
#define SD_MISO_D0_PIN      GPIO_NUM_40

/*i2c dev*/
#define I2C_RTC_DEV_Address        0x51
#define I2C_SHTC3_DEV_Address      0x70
#define I2C_FT6336_DEV_Address     0x38

/*---- reseau ----*/
/*---- reseau : SSID + mot de passe dans secrets.h (fichier non versionne).
   Copier secrets.h.example en secrets.h et le remplir. ----*/
#include "secrets.h"
#define WIFI_MAX_RETRY 4

/*---- LLM (Groq) ----*/
/* Tout passe par Groq : tres rapide et gratuit (cle sur console.groq.com/keys).
   La carte a 8 Mo de flash / 8 Mo de PSRAM : aucun modele de reconnaissance
   vocale ne tient dedans, donc la reconnaissance est un appel reseau. */
#define GROQ_BASE      "https://api.groq.com/openai/v1"
#define STT_URL        GROQ_BASE "/audio/transcriptions"
#define STT_MODEL      "whisper-large-v3-turbo"
#define GROQ_LLM_URL   GROQ_BASE "/chat/completions"
#define GROQ_LLM_MODEL "llama-3.3-70b-versatile"

/*---- cerveau : DeepSeek (cle deja dans secrets.h) ----
   Utilise si aucune cle Groq n'est fournie : comme ca le robot parle
   des le premier flash, sans inscription. */
#define DS_URL   "https://api.deepseek.com/chat/completions"
#define DS_MODEL "deepseek-flash"   /* DeepSeek V4.1 Flash */

/*---- voix de sortie : TTS de Google Traduction, gratuit et sans cle ----*/
#define TTS_URL   "https://translate.google.com/translate_tts?ie=UTF-8&client=tw-ob&tl=fr&q="
/* meme adresse, mais la langue est ajoutee a la fin (variable `langue`) */
#define TTS_DEBUT "https://translate.google.com/translate_tts?ie=UTF-8&client=tw-ob&q="
/* les titres du moment (flux RSS) pour la page INFOS */
/* SAFARI : la page chargee au demarrage (mode texte, HTTPS verifie) */
#ifndef SAFARI_URL
#define SAFARI_URL "https://www.oodrive.com/"
#endif

#define INFOS_URL "https://www.franceinfo.fr/titres.rss"   /* l'ancien domaine redirige en 301, et http_get ne suit pas les redirections */
#define TTS_UA    "Mozilla/5.0 (Linux; Android 10) AppleWebKit/537.36 Chrome/120 Mobile Safari/537.36"

/*---- meteo (Open-Meteo, gratuit et sans cle) ----*/
#define METEO_VILLE "BAGNOLET"
#define METEO_LAT   "48.8724"
#define METEO_LON   "2.4173"
#define METEO_URL   "https://api.open-meteo.com/v1/forecast?latitude=" METEO_LAT \
                    "&longitude=" METEO_LON "&current=temperature_2m,weather_code" \
                    "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max" \
                    "&forecast_days=2&timezone=Europe%2FParis"
#define METEO_PERIODE_S 900          /* rafraichie toutes les 15 min (Open-Meteo
                                        ne bouge pas plus vite) */

#endif // !USER_CONFIG_H
