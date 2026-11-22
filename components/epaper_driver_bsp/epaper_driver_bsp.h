#ifndef EPAPER_DRIVER_BSP_H
#define EPAPER_DRIVER_BSP_H

#include "driver/spi_master.h"
#include "driver/gpio.h"

/* Display color */
typedef enum {
    DRIVER_COLOR_WHITE  = 0xff,
    DRIVER_COLOR_BLACK  = 0x00,
    FONT_BACKGROUND = DRIVER_COLOR_WHITE,
}COLOR_IMAGE;

typedef struct {
    uint8_t cs;
    uint8_t dc;
    uint8_t rst;
    uint8_t busy;
    uint8_t mosi;
    uint8_t scl;
    int spi_host;
    int buffer_len;
}custom_lcd_spi_t;

class epaper_driver_display {
private:
    const custom_lcd_spi_t lcd_spi_data;
    const int Width;
    const int Height;
    spi_device_handle_t spi;
    uint8_t *buffer = NULL;

    void spi_gpio_init();
    void spi_port_init();
    void read_busy();

    void set_cs_1(){gpio_set_level((gpio_num_t)lcd_spi_data.cs,1);}
    void set_cs_0(){gpio_set_level((gpio_num_t)lcd_spi_data.cs,0);}
    void set_dc_1(){gpio_set_level((gpio_num_t)lcd_spi_data.dc,1);}
    void set_dc_0(){gpio_set_level((gpio_num_t)lcd_spi_data.dc,0);}
    void set_rst_1(){gpio_set_level((gpio_num_t)lcd_spi_data.rst,1);}
    void set_rst_0(){gpio_set_level((gpio_num_t)lcd_spi_data.rst,0);}

    void SPI_SendByte(uint8_t data);
    void EPD_SendData(uint8_t data);
    void EPD_SendCommand(uint8_t command);
    void writeBytes(uint8_t *buffer,int len);
    void writeBytes(const uint8_t *buffer, int len);
    void EPD_SetWindows(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend);
    void EPD_SetCursor(uint16_t Xstart, uint16_t Ystart);
    void EPD_SetLut(const uint8_t *lut);
    void EPD_TurnOnDisplay();
    void EPD_TurnOnDisplayPart();

public:
    epaper_driver_display(int width, int height,custom_lcd_spi_t _lcd_spi_data);
    ~epaper_driver_display();

    void EPD_Init();    /* 墨水屏初始化 */
    void EPD_Clear();   /* 清空屏幕 */
    void EPD_Display(); /* 刷buffer到墨水屏 */
    
    /*局部刷新*/
    void EPD_DisplayPartBaseImage();
    void EPD_Init_Partial();
    void EPD_DisplayPart();
    void EPD_DrawColorPixel(uint16_t x, uint16_t y,uint8_t color);

    /* envoi direct d'une framebuffer packee 200x200 (1 bit/px, 1=blanc) */
    void EPD_ShowPart(const uint8_t *fb, int len);

    /* rafraichissement PARTIEL d'une bande : n'envoie que la fenetre demandee
       (y0..y1 inclus), fb etant une image de `stride` octets par ligne.
       1=blanc, 0=noir. x0 est aligne sur 8. Renvoie le nombre d'octets envoyes. */
    int EPD_ShowPartWindow(const uint8_t *fb, int x0, int y0, int x1, int y1, int stride);
    int busy_ko = 0;   /* nombre de fois ou la dalle est restee occupee */
    /* LA DALLE EST-ELLE BLOQUEE ? Si oui, plus AUCUNE fonction publique ne
       parle a la dalle : sinon chaque appel repart pour 8 s de BUSY et la
       dalle reste definitivement ficelee (ecran qui clignote en boucle). */
    volatile bool panne = false;
    void EPD_Panne_Reset() { panne = false; busy_ko = 0; }
    /* idem, avec le choix de la sequence 0x22 (0xCF lent, 0xC7 rapide) */
    int EPD_ShowPartWindowSeq(const uint8_t *fb, int x0, int y0, int x1, int y1,
                              int stride, uint8_t seq);
    void EPD_ShowFull(const uint8_t *fb, int len);
    /* image de base : les deux RAM (0x24 + 0x26) recoivent la meme image,
       puis rafraichissement complet. Sans ca, les mises a jour partielles
       comparent le nouveau dessin a une RAM restee a l'etat d'usine -> fantomes. */
    void EPD_ShowBase(const uint8_t *fb, int len);
    /* rafraichissement complet "propre" : recharge la LUT pleine, reecrit les
       deux RAM, rafraichit, puis rearme le mode partiel. C'est ce qu'il faut
       appeler a chaque changement de page pour effacer les traces. */
    void EPD_ShowFullClean(const uint8_t *fb, int len);
    void EPD_EnterPartial(void);
};
#endif
