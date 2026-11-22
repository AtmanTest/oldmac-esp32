/*  art.cpp - BATMino en 1 bit, partage firmware / hote.
 *  Aucune dependance ESP-IDF : compile tel quel des deux cotes.
 */
#include "art.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* ---------------------------------------------------------------- */
/* canevas + inclinaison                                            */
/* ---------------------------------------------------------------- */
static uint8_t *g_fb = 0;
static int g_oy = 0;
static int g_tilt = 0;              /* 0 = pas d'inclinaison */
static float g_cos = 1.0f, g_sin = 0.0f;
static int g_tcx = 100, g_tcy = 88;

void art_begin(uint8_t *fb, int oy) { art_tables_init(); g_fb = fb; g_oy = oy; g_tilt = 0; }
uint8_t *art_fb(void) { return g_fb; }
void art_clear(void) { if (g_fb) memset(g_fb, 0xFF, ART_FB_LEN); }

void art_tilt(float deg, int cx, int cy)
{
    g_tilt = (fabsf(deg) > 0.01f) ? 1 : 0;
    float r = deg * M_PI / 180.0f;
    g_cos = cosf(r);
    g_sin = sinf(r);
    g_tcx = cx; g_tcy = cy;
}

void art_no_tilt(void) { g_tilt = 0; }

static inline void raw_px(int x, int y, int black)
{
    int i, m;
    if (x < 0 || x >= ART_W || y < 0 || y >= ART_H || !g_fb) return;
    i = y * 25 + (x >> 3);
    m = 0x80 >> (x & 7);
    if (black) g_fb[i] &= (uint8_t)~m; else g_fb[i] |= (uint8_t)m;
}

void a_px(int x, int y, int black)
{
    if (g_tilt) {
        int dx = x - g_tcx, dy = y - g_tcy;
        int rx = g_tcx + (int)lroundf((float)dx * g_cos - (float)dy * g_sin);
        int ry = g_tcy + (int)lroundf((float)dx * g_sin + (float)dy * g_cos);
        raw_px(rx, ry + g_oy, black);
    } else {
        raw_px(x, y + g_oy, black);
    }
}

/* ---------------------------------------------------------------- */
/* primitives                                                       */
/* ---------------------------------------------------------------- */
void a_hline(int x0, int x1, int y, int t)
{
    int x, k;
    if (x1 < x0) { int s = x0; x0 = x1; x1 = s; }
    for (x = x0; x <= x1; x++) for (k = 0; k < t; k++) a_px(x, y + k, 1);
}

void a_vline(int x, int y0, int y1, int t)
{
    int y, k;
    if (y1 < y0) { int s = y0; y0 = y1; y1 = s; }
    for (y = y0; y <= y1; y++) for (k = 0; k < t; k++) a_px(x + k, y, 1);
}

void a_disc(int cx, int cy, int r, int black)
{
    int x, y;
    for (y = -r; y <= r; y++) for (x = -r; x <= r; x++)
        if (x * x + y * y <= r * r) a_px(cx + x, cy + y, black);
}

void a_ring(int cx, int cy, int r, int t, int black)
{
    int x, y;
    for (y = -r - t; y <= r + t; y++) for (x = -r - t; x <= r + t; x++) {
        int d2 = x * x + y * y;
        if (d2 <= (r + t) * (r + t) && d2 >= (r - t) * (r - t)) a_px(cx + x, cy + y, black);
    }
}

void a_efill(int cx, int cy, int rx, int ry, int black)
{
    int x, y;
    if (rx <= 0 || ry <= 0) return;
    for (y = -ry; y <= ry; y++) for (x = -rx; x <= rx; x++) {
        float v = (float)(x * x) / (float)(rx * rx) + (float)(y * y) / (float)(ry * ry);
        if (v <= 1.0f) a_px(cx + x, cy + y, black);
    }
}

void a_ering(int cx, int cy, int rx, int ry, int t, int black)
{
    int x, y, ix = rx - t, iy = ry - t;
    if (ix < 1) ix = 1;
    if (iy < 1) iy = 1;
    for (y = -ry; y <= ry; y++) for (x = -rx; x <= rx; x++) {
        float v = (float)(x * x) / (float)(rx * rx) + (float)(y * y) / (float)(ry * ry);
        float vi = (float)(x * x) / (float)(ix * ix) + (float)(y * y) / (float)(iy * iy);
        if (v <= 1.0f && vi >= 1.0f) a_px(cx + x, cy + y, black);
    }
}

void a_seg(int x0, int y0, int x1, int y1, int t)
{
    int dx = abs(x1 - x0), dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        a_disc(x0, y0, t, 1);
        if (x0 == x1 && y0 == y1) break;
        { int e2 = 2 * err;
          if (e2 >= dy) { err += dy; x0 += sx; }
          if (e2 <= dx) { err += dx; y0 += sy; } }
    }
}

void a_arc(int cx, int cy, int r, float a0, float a1, int t)
{
    float step = (r > 40) ? 0.7f : 1.2f, a;
    for (a = a0; a <= a1; a += step) {
        float rad = a * M_PI / 180.0f;
        a_disc(cx + (int)lroundf(r * cosf(rad)), cy - (int)lroundf(r * sinf(rad)), t, 1);
    }
}

/* meme chose que a_arc mais avec la couleur choisie (0 = blanc, 1 = noir) */
void a_arc_c(int cx, int cy, int r, float a0, float a1, int t, int black)
{
    float step = (r > 40) ? 0.7f : 1.2f, a;
    for (a = a0; a <= a1; a += step) {
        float rad = a * M_PI / 180.0f;
        a_disc(cx + (int)lroundf(r * cosf(rad)), cy - (int)lroundf(r * sinf(rad)), t, black);
    }
}

void a_arcf(int cx, int cy, int rx, int ry, float a0, float a1, int t)
{
    float a;
    for (a = a0; a <= a1; a += 1.0f) {
        float rad = a * M_PI / 180.0f;
        a_disc(cx + (int)lroundf(rx * cosf(rad)), cy - (int)lroundf(ry * sinf(rad)), t, 1);
    }
}

void a_tri(int x0, int y0, int x1, int y1, int x2, int y2, int black)
{
    int minx = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
    int maxx = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
    int miny = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
    int maxy = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
    int x, y;
    for (y = miny; y <= maxy; y++) for (x = minx; x <= maxx; x++) {
        float d1 = (float)((x1 - x0) * (y - y0) - (y1 - y0) * (x - x0));
        float d2 = (float)((x2 - x1) * (y - y1) - (y2 - y1) * (x - x1));
        float d3 = (float)((x0 - x2) * (y - y2) - (y0 - y2) * (x - x2));
        int neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
        int pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
        if (!(neg && pos)) a_px(x, y, black);
    }
}

void a_wedge(int cx, int cy, int rx, int ry, float a0, float a1, int black)
{
    int x, y;
    for (y = -ry; y <= ry; y++) for (x = -rx; x <= rx; x++) {
        float v = (float)(x * x) / (float)(rx * rx) + (float)(y * y) / (float)(ry * ry);
        if (v > 1.0f) continue;
        { float ang = atan2f((float)-y, (float)x) * 180.0f / M_PI;
          if (ang < 0.0f) ang += 360.0f;
          { float b0 = a0, b1 = a1;
            while (b0 < 0.0f) { b0 += 360.0f; b1 += 360.0f; }
            while (b0 >= 360.0f) { b0 -= 360.0f; b1 -= 360.0f; }
            if (ang >= b0 && ang <= b1) a_px(cx + x, cy + y, black); } }
    }
}

/* ---------------------------------------------------------------- */
/* police 3x5                                                       */
/* ---------------------------------------------------------------- */
static const uint8_t DIG5[10][5] = {
    {7, 5, 5, 5, 7}, {2, 2, 2, 2, 2}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1},
    {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 1, 1}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7}
};
static const uint8_t GC5[5] = {7, 4, 4, 4, 7};      /* C */
static const uint8_t GP5[5] = {5, 1, 2, 4, 5};      /* % */
static const uint8_t GM5[5] = {4, 5, 7, 5, 5};      /* M */

static const char FONT_CHARS[] = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ.:-/%!?#()+=<>,*'&_";
static const uint8_t FONT3X5[56][5] = {
  {0,0,0,0,0}, {7,5,5,5,7}, {2,2,2,2,2}, {7,1,7,4,7}, {7,1,7,1,7}, {5,5,7,1,1},
  {7,4,7,1,7}, {7,4,7,5,7}, {7,1,1,1,1}, {7,5,7,5,7}, {7,5,7,1,7}, {2,5,7,5,5},
  {6,5,6,5,6}, {7,4,4,4,7}, {6,5,5,5,6}, {7,4,6,4,7}, {7,4,6,4,4}, {7,4,5,5,7},
  {5,5,7,5,5}, {7,2,2,2,7}, {1,1,1,5,7}, {5,5,6,5,5}, {4,4,4,4,7}, {5,7,7,5,5},
  {7,5,5,5,5}, {7,5,5,5,7}, {7,5,7,4,4}, {7,5,5,7,1}, {7,5,7,6,5}, {7,4,7,1,7},
  {7,2,2,2,2}, {5,5,5,5,7}, {5,5,5,5,2}, {5,5,7,7,5}, {5,5,2,5,5}, {5,5,2,2,2},
  {7,1,2,4,7}, {0,0,0,0,2}, {0,2,0,2,0}, {0,0,7,0,0}, {1,1,2,4,4}, {5,1,2,4,5},
  {2,2,2,0,2}, {7,1,3,0,2}, {5,7,5,7,5}, {2,4,4,4,2}, {2,1,1,1,2}, {0,2,7,2,0},
  {0,7,0,7,0}, {1,2,4,2,1}, {4,2,1,2,4}, {0,0,0,2,4}, {5,2,5,0,0}, {2,2,0,0,0},
  {2,5,2,5,2}, {0,0,0,0,7},
};

static void g3x5(int x, int y, int s, const uint8_t rows[5], int black)
{
    int r, c, dy, dx;
    for (r = 0; r < 5; r++) for (c = 0; c < 3; c++)
        if (rows[r] & (4 >> c))
            for (dy = 0; dy < s; dy++) for (dx = 0; dx < s; dx++)
                a_px(x + c * s + dx, y + r * s + dy, black);
}

static int font_idx(char c)
{
    int i;
    if (c >= 'a' && c <= 'z') c = (char)(c - 32);
    /* quelques accents francais ramenes a la lettre de base */
    if ((unsigned char)c == 0xC3) return -1;     /* UTF-8 : ignore le premier octet */
    for (i = 0; i < (int)sizeof(FONT_CHARS) - 1; i++) if (FONT_CHARS[i] == c) return i;
    return -1;
}

/* black = 1 texte noir sur fond clair, 0 texte blanc sur fond noir (menu) */
int a_text_col(int x, int y, int s, const char *str, int black)
{
    int i = 0;
    const char *p;
    for (p = str; *p; p++) {
        int k = font_idx(*p);
        if (k < 0) continue;                      /* octet de suite UTF-8 : saute */
        g3x5(x + i * 4 * s, y, s, FONT3X5[k], black);
        i++;
    }
    return i > 0 ? i * 4 * s - s : 0;
}

int a_text(int x, int y, int s, const char *str) { return a_text_col(x, y, s, str, 1); }

int a_num(int x, int y, int s, int val, int min_d)
{
    char b[8];
    int n = 0, v = val < 0 ? -val : val, i;
    do { b[n++] = (char)('0' + (v % 10)); v /= 10; } while (v && n < 7);
    while (n < min_d && n < 7) b[n++] = '0';
    for (i = 0; i < n; i++) g3x5(x + i * 4 * s, y, s, DIG5[b[n - 1 - i] - '0'], 1);
    return n * 4 * s - s;
}

int a_text_w(int s, const char *str)
{
    int i = 0;
    for (; *str; str++) if (font_idx(*str) >= 0) i++;
    return i > 0 ? i * 4 * s - s : 0;
}

int a_num_w(int s, int val, int min_d)
{
    int n = 0, v = val < 0 ? -val : val;
    do { n++; v /= 10; } while (v && n < 7);
    return (n < min_d ? min_d : n) * 4 * s - s;
}

int a_text_c(int cx, int y, int s, const char *str)
{
    int w = a_text_w(s, str);
    return a_text(cx - w / 2, y, s, str);
}

/* ---------------------------------------------------------------- */
/* icones                                                           */
/* ---------------------------------------------------------------- */
void ic_wifi(int cx, int cy, int on)
{
    int b = cy + 4;
    a_arc(cx, b, 11, 35, 145, 1);
    a_arc(cx, b, 7, 35, 145, 1);
    a_arc(cx, b, 3, 35, 145, 1);
    a_disc(cx, b - 1, 1, 1);
    if (!on) a_seg(cx - 9, cy - 11, cx + 9, cy + 9, 1);
}

void ic_temp(int cx, int cy)
{
    a_ring(cx, cy + 4, 3, 1, 1);
    a_seg(cx, cy - 5, cx, cy + 3, 1);
    a_px(cx - 1, cy - 5, 1); a_px(cx + 1, cy - 5, 1);
}

void ic_gear(int cx, int cy, int r, int t)
{
    int i;
    for (i = 0; i < 8; i++) {
        float a = (float)i * 45.0f * M_PI / 180.0f;
        a_seg(cx + (int)lroundf((r + 1) * cosf(a)), cy + (int)lroundf((r + 1) * sinf(a)),
              cx + (int)lroundf((r + t) * cosf(a)), cy + (int)lroundf((r + t) * sinf(a)), 1);
    }
    a_ring(cx, cy, r, t, 1);
}

void ic_heart(int cx, int cy, int s, int filled)
{
    if (filled) {
        a_disc(cx - s / 2, cy - s / 3, s / 2 + 1, 1);
        a_disc(cx + s / 2, cy - s / 3, s / 2 + 1, 1);
        a_tri(cx - s, cy, cx + s, cy, cx, cy + s + 2, 1);
    } else {
        a_ring(cx - s / 2, cy - s / 3, s / 2 + 1, 1, 1);
        a_ring(cx + s / 2, cy - s / 3, s / 2 + 1, 1, 1);
        a_seg(cx - s, cy, cx, cy + s + 2, 1);
        a_seg(cx + s, cy, cx, cy + s + 2, 1);
    }
}

void ic_food(int cx, int cy)
{
    a_efill(cx, cy + 4, 8, 4, 1);      /* assiette */
    a_hline(cx - 8, cx + 8, cy + 8, 1);
    a_seg(cx, cy - 9, cx, cy + 3, 1);  /* couvert */
    a_disc(cx, cy - 10, 2, 1);
}

void ic_bolt(int cx, int cy, int s)
{
    a_tri(cx, cy - s, cx - s, cy + 2, cx + s / 2, cy + 1, 1);
    a_tri(cx, cy + s, cx + s, cy - 2, cx - s / 2, cy - 1, 1);
}

void ic_chip(int cx, int cy)
{
    int i;
    a_ring(cx, cy, 8, 2, 1);
    for (i = -6; i <= 6; i += 4) {
        a_seg(cx + i, cy - 12, cx + i, cy - 8, 1);
        a_seg(cx + i, cy + 8, cx + i, cy + 12, 1);
        a_seg(cx - 12, cy + i, cx - 8, cy + i, 1);
        a_seg(cx + 8, cy + i, cx + 12, cy + i, 1);
    }
}

void ic_ram(int cx, int cy)
{
    int i;
    a_hline(cx - 11, cx + 11, cy + 4, 2);
    a_hline(cx - 11, cx + 11, cy - 4, 2);
    for (i = -8; i <= 8; i += 4) a_vline(cx + i, cy - 2, cy + 2, 1);
}

void ic_mic(int cx, int cy, int s)
{
    a_efill(cx, cy - s, 3 * s / 5 + 1, s, 1);
    a_arc(cx, cy - s / 3, s + 1, 200, 340, 1);
    a_vline(cx, cy + s, cy + s + 3, 1);
    a_hline(cx - 4, cx + 4, cy + s + 4, 1);
}

void ic_bat_c(int cx, int cy, int s, int black)
{
    a_efill(cx, cy, s / 3 + 1, s, black);
    a_tri(cx - 2, cy - s, cx - s, cy - s / 2, cx - s / 3, cy + s / 3, black);
    a_tri(cx + 2, cy - s, cx + s, cy - s / 2, cx + s / 3, cy + s / 3, black);
    a_tri(cx - s, cy - s / 2, cx - s + s / 3, cy - s / 2 - s / 3, cx - s / 3, cy + s / 3, black);
    a_tri(cx + s, cy - s / 2, cx + s - s / 3, cy - s / 2 - s / 3, cx + s / 3, cy + s / 3, black);
    a_tri(cx - 2, cy - s, cx - 4, cy - s - s / 2, cx, cy - s, black);
    a_tri(cx + 2, cy - s, cx + 4, cy - s - s / 2, cx, cy - s, black);
}

void ic_bat(int cx, int cy, int s)
{
    /* logo : corps + ailes en accent circonflexe */
    a_efill(cx, cy, s / 3 + 1, s, 1);
    a_tri(cx - 2, cy - s, cx - s, cy - s / 2, cx - s / 3, cy + s / 3, 1);
    a_tri(cx + 2, cy - s, cx + s, cy - s / 2, cx + s / 3, cy + s / 3, 1);
    a_tri(cx - s, cy - s / 2, cx - s + s / 3, cy - s / 2 - s / 3, cx - s / 3, cy + s / 3, 1);
    a_tri(cx + s, cy - s / 2, cx + s - s / 3, cy - s / 2 - s / 3, cx + s / 3, cy + s / 3, 1);
    /* oreilles */
    a_tri(cx - 2, cy - s, cx - 4, cy - s - s / 2, cx, cy - s, 1);
    a_tri(cx + 2, cy - s, cx + 4, cy - s - s / 2, cx, cy - s, 1);
}

void ic_speaker(int cx, int cy)
{
    a_tri(cx - 9, cy - 4, cx - 9, cy + 4, cx - 2, cy + 9, 1);
    a_tri(cx - 9, cy - 4, cx - 2, cy - 9, cx - 2, cy + 9, 1);
    a_arc(cx - 1, cy, 9, 300, 60, 1);
    a_arc(cx - 1, cy, 14, 310, 50, 1);
}

void ic_power(int cx, int cy, int r)
{
    a_arc(cx, cy, r, 110, 430, 2);
    a_vline(cx - 1, cy - r - 4, cy - 2, 2);
}

void ic_moon(int cx, int cy, int r)
{
    a_disc(cx, cy, r, 1);
    a_disc(cx + r / 2, cy - r / 3, r, 0);
}

/* ---------------------------------------------------------------- */
/* BATMino : la tete                                                */
/* ---------------------------------------------------------------- */
#define HX 100
#define HY 100
#define HRX 43
#define HRY 43

void ic_bat_small(int cx, int cy, int s)
{
    a_disc(cx, cy, s, 1);
    a_tri(cx - s + 1, cy - s / 2, cx - s / 2, cy - s * 2, cx + 1, cy - s, 1);
    a_tri(cx + s - 1, cy - s / 2, cx + s / 2, cy - s * 2, cx - 1, cy - s, 1);
    a_efill(cx - s / 2, cy + s / 4, s / 4, s / 5, 0);
    a_efill(cx + s / 2, cy + s / 4, s / 4, s / 5, 0);
}

/* un oeil : amande blanche taillee dans la cagoule.
 * Le trait le plus expressif du visage : largeur, hauteur, inclinaison
 * (coin exterieur releve = fierte / abaisse = tristesse) et paupieres. */
static void almond(int cx, int cy, int rx, int ryu, int ryd, float slope, int cut_top, int cut_bot)
{
    int x, y, yt, yb;
    for (x = -rx; x <= rx; x++) {
        float t = (float)x / (float)rx;
        float sh = powf(sqrtf(1.0f - t * t), 0.75f);
        yt = cy + (int)lroundf(-(float)ryu * sh + slope * (float)x) + cut_top;
        yb = cy + (int)lroundf((float)ryd * sh + slope * (float)x) - cut_bot;
        for (y = yt; y <= yb; y++) a_px(cx + x, y, 0);   /* blanc */
    }
}

static void eye(int cx, int cy, int code, int brow, int mirror)
{
    float sg = mirror ? -1.0f : 1.0f;
    int rx = 15, ryu = 9, ryd = 7, ct = 0, cb = 0, px = 0, py = 0, pupille = 1;
    float slope = 0.08f * sg;

    switch (code) {
    case EYE_WIDE:   rx = 16; ryu = 12; ryd = 10; slope = 0.0f; break;
    case EYE_NARROW: ryu = 5;  ryd = 4;  slope = 0.16f * sg; break;
    case EYE_HALF:   ryu = 12; ryd = 7;  ct = 5; break;
    case EYE_ANGRY:  rx = 16; ryu = 9;   ryd = 6; slope = 0.30f * sg; ct = 3; break;
    case EYE_SCARED: rx = 13; ryu = 13;  ryd = 11; slope = 0.0f; break;
    case EYE_TEARY:  rx = 15; ryu = 11;  ryd = 9; ct = 3; break;
    case EYE_TINY:   break;
    case EYE_GLOW:   pupille = 0; break;
    default: break;
    }

    /* humeur generale lue dans la cagoule */
    if (brow == BROW_ANGRY)      { slope += 0.14f * sg; ct += 2; }
    if (brow == BROW_ANGRY_HARD) { slope += 0.24f * sg; ct += 3; }
    if (brow == BROW_SAD)        { slope -= 0.16f * sg; }
    if (brow == BROW_UP)         { ryu += 2; ryd += 1; slope *= 0.4f; }
    if (brow == BROW_DET)        { ct += 2; slope *= 0.2f; }

    switch (code) {
    case EYE_HAPPY:                    /* croissant bas : oeil rieur, blanc garde */
        almond(cx, cy + 2, 15, 11, 11, 0.10f * sg, 0, 0);
        a_arc(cx, cy + 2, 15, 200, 340, 1);
        a_hline(cx - 16, cx + 16, cy + 1, 1);
        return;
    case EYE_SAD:                      /* croissant haut : oeil qui pleure */
        almond(cx, cy - 2, 15, 10, 10, -0.16f * sg, 0, 0);
        a_hline(cx - 15, cx + 15, cy - 1, 1);
        a_arc(cx, cy - 2, 15, 20, 160, 1);
        return;
    case EYE_LINE:
        a_hline(cx - 13, cx + 13, cy + 1, 3);
        return;
    case EYE_CROSS:
        a_seg(cx - 11, cy - 9, cx + 11, cy + 9, 2);
        a_seg(cx - 11, cy + 9, cx + 11, cy - 9, 2);
        return;
    case EYE_WINK:                     /* clin : un oeil ferme malin */
        almond(cx, cy, rx, 4, 4, -0.18f * sg, 0, 0);
        a_hline(cx - rx, cx + rx, cy + 4, 2);
        return;
    case EYE_STAR:                     /* pupille etoile dans l'amande blanche */
        almond(cx, cy, rx, ryu, ryd, slope, ct, cb);
        a_seg(cx, cy - 8, cx, cy + 8, 2); a_seg(cx - 8, cy, cx + 8, cy, 2);
        a_seg(cx - 5, cy - 5, cx + 5, cy + 5, 1); a_seg(cx - 5, cy + 5, cx + 5, cy - 5, 1);
        return;
    case EYE_HEART:
        almond(cx, cy, rx, ryu, ryd, slope, ct, cb);
        ic_heart(cx, cy + 1, 7, 1);
        return;
    default: break;
    }

    switch (code) {
    case EYE_LOOK_L:    px = -5; break;
    case EYE_LOOK_R:    px =  5; break;
    case EYE_LOOK_UP:   py = -4; break;
    case EYE_LOOK_DOWN: py =  4; break;
    case EYE_ROLL:      py = -4; px = 4; break;
    case EYE_TEARY:     px = 2;  py = 3; break;
    default: break;
    }

    almond(cx, cy, rx, ryu, ryd, slope, ct, cb);
    if (code == EYE_TINY) {              /* pupille minuscule : effroi */
        a_disc(cx + (mirror ? -2 : 2), cy, 3, 1);
        a_disc(cx + (mirror ? -3 : 1), cy - 1, 1, 0);
        return;
    }
    if (brow == BROW_DET) {            /* arete du sourcil taillee en blanc */
        int k;
        for (k = -rx + 3; k <= rx - 3; k++)
            for (int q = 0; q < 2; q++)
                a_px(cx + k, cy - ryu - 3 + (int)lroundf(slope * (float)k) + q, 0);
    }
    if (brow == BROW_ANGRY_HARD) {     /* pli du front */
        a_seg(cx - 6 * (mirror ? -1 : 1), cy - 22, cx + 2 * (mirror ? -1 : 1), cy - 30, 1);
    }
    if (pupille) {
        int pxc = cx + px, pyc = cy + py + (int)lroundf(slope * (float)px);
        a_disc(pxc, pyc, 5, 1);
        a_disc(pxc - 2, pyc - 2, 1, 0);      /* reflet */
        a_disc(pxc + 2, pyc + 2, 1, 0);
    }
}

/* la cagoule : crane, oreilles, museau, arete du nez */
static void cowl(int brow)
{
    int apex = HY + 12;

    /* crane + sommet legerement plat (moins rond, plus casque) */
    a_efill(HX, HY + 2, HRX, HRY - 2, 1);
    a_tri(HX - HRX + 6, HY - HRY + 12, HX + HRX - 6, HY - HRY + 12,
          HX + HRX, HY - HRY + 30, 1);
    a_tri(HX - HRX + 6, HY - HRY + 12, HX - HRX, HY - HRY + 30,
          HX + HRX, HY - HRY + 30, 1);

    /* oreilles longues, larges a la base, pointe nette */
    a_tri(HX - 42, HY - 18, HX - 26, HY - 74, HX - 8, HY - 34, 1);
    a_tri(HX + 42, HY - 18, HX + 26, HY - 74, HX + 8, HY - 34, 1);
    a_tri(HX - 34, HY - 24, HX - 27, HY - 62, HX - 15, HY - 34, 0);
    a_tri(HX + 34, HY - 24, HX + 27, HY - 62, HX + 15, HY - 34, 0);

    /* museau : bas du visage decouvert, oval + menton */
    a_efill(HX, HY + 22, 25, 15, 0);
    a_efill(HX, HY + 30, 16, 10, 0);

    /* arete du nez : la cagoule descend en pointe entre les yeux */
    if (brow == BROW_UP)        apex = HY + 2;
    else if (brow == BROW_ANGRY) apex = HY + 16;
    else if (brow == BROW_ANGRY_HARD) apex = HY + 20;
    else if (brow == BROW_SAD)  apex = HY + 6;
    a_tri(HX - 10, HY - 20, HX + 10, HY - 20, HX, apex, 1);

    /* contour : trait franc autour du crane et des oreilles */
    a_ering(HX, HY + 2, HRX, HRY - 2, 2, 1);

    /* col de cape : deux pointes sous la machoire (signature Batman) */
    a_tri(HX - 30, HY + 34, HX - 44, HY + 46, HX - 12, HY + 44, 1);
    a_tri(HX + 30, HY + 34, HX + 44, HY + 46, HX + 12, HY + 44, 1);
}

void a_symbole(int code, int x, int y)
{
    switch (code) {
    case X_HEART: ic_heart(x, y, 8, 1); break;
    case X_STAR:
        a_seg(x, y - 10, x, y + 10, 1); a_seg(x - 10, y, x + 10, y, 1);
        a_seg(x - 7, y - 7, x + 7, y + 7, 1); a_seg(x - 7, y + 7, x + 7, y - 7, 1);
        break;
    case X_ZZZ:
        a_text(x, y, 2, "Z"); a_text(x + 12, y - 14, 1, "Z"); a_text(x + 20, y - 24, 1, "Z");
        break;
    case X_SWEAT:
        a_disc(x, y + 4, 5, 1); a_tri(x - 5, y + 4, x + 5, y + 4, x, y - 10, 1);
        a_disc(x - 1, y + 3, 1, 0);
        break;
    case X_ANGER:
        a_seg(x - 8, y - 8, x + 8, y + 8, 2); a_seg(x + 8, y - 8, x - 8, y + 8, 2);
        a_seg(x - 14, y - 6, x - 7, y + 1, 2); a_seg(x - 6, y - 14, x + 1, y - 7, 2);
        break;
    case X_QUESTION:
        a_arc(x, y - 5, 8, 20, 200, 2); a_vline(x + 8, y - 2, y + 4, 2); a_disc(x + 8, y + 11, 2, 1);
        break;
    case X_SPARK:
        a_seg(x - 8, y, x + 8, y, 1); a_seg(x, y - 8, x, y + 8, 1);
        a_seg(x - 5, y - 5, x + 5, y + 5, 1);
        break;
    case X_MUSIC:
        a_vline(x - 6, y - 8, y + 6, 2); a_vline(x + 5, y - 11, y + 3, 2);
        a_hline(x - 6, x + 5, y - 10, 2); a_disc(x - 9, y + 7, 4, 1); a_disc(x + 1, y + 4, 4, 1);
        break;
    case X_DROP:
        a_disc(x, y + 3, 5, 1); a_tri(x - 5, y + 3, x + 5, y + 3, x, y - 9, 1);
        break;
    case X_SKULL:
        a_efill(x, y - 3, 9, 8, 1); a_efill(x - 4, y - 4, 3, 3, 0); a_efill(x + 4, y - 4, 3, 3, 0);
        a_hline(x - 5, x + 5, y + 8, 2);
        break;
    case X_FIRE:
        a_tri(x - 7, y + 8, x + 7, y + 8, x, y - 6, 1);
        a_tri(x - 3, y + 8, x + 5, y + 8, x + 2, y - 12, 1);
        a_disc(x - 1, y + 5, 2, 0);
        break;
    case X_VEIN:                       /* veines de colere */
        a_seg(x - 10, y - 8, x - 2, y + 4, 2);
        a_seg(x - 2, y + 4, x + 8, y - 10, 2);
        a_seg(x + 8, y - 10, x + 10, y + 2, 2);
        a_seg(x - 6, y - 12, x - 2, y - 4, 1);
        a_seg(x + 4, y + 2, x + 10, y + 8, 1);
        break;
    case X_SPIRAL:                     /* tete qui tourne */
        a_arc(x, y, 10, 0, 300, 2);
        a_arc(x, y, 6, 60, 330, 2);
        a_arc(x, y, 2, 120, 400, 2);
        break;
    case X_HATCH:                      /* cernes / deprime : hachures */
        a_seg(x - 10, y - 6, x - 6, y + 6, 1);
        a_seg(x - 4, y - 6, x, y + 6, 1);
        a_seg(x + 2, y - 6, x + 6, y + 6, 1);
        break;
    case X_CROC:                       /* crocs */
        a_tri(x - 10, y - 3, x - 3, y + 6, x - 3, y - 3, 1);
        a_tri(x + 10, y - 3, x + 3, y + 6, x + 3, y - 3, 1);
        break;
    case X_DOTS:                       /* bulle de pensee : ... */
        a_disc(x - 10, y, 2, 1); a_disc(x, y, 2, 1); a_disc(x + 10, y, 2, 1);
        break;
    case X_PAIN:                       /* eclairs de douleur */
        a_seg(x - 8, y - 10, x - 3, y - 1, 2);
        a_seg(x - 3, y - 1, x - 9, y + 1, 2);
        a_seg(x - 9, y + 1, x - 2, y + 10, 2);
        a_seg(x + 8, y - 10, x + 3, y - 1, 2);
        a_seg(x + 3, y - 1, x + 9, y + 1, 2);
        a_seg(x + 9, y + 1, x + 2, y + 10, 2);
        break;
    case X_MOTION:                     /* tremblement / sursaut */
        a_seg(x - 12, y - 8, x - 6, y - 12, 2);
        a_seg(x - 10, y + 2, x - 4, y - 2, 2);
        a_seg(x - 12, y + 12, x - 6, y + 8, 2);
        break;
    default: break;
    }
}

static void mouth(int code, int cx, int cy)
{
    switch (code) {
    case M_SMILE:     a_arc(cx, cy - 6, 17, 205, 335, 2); break;
    case M_BIG_SMILE: a_wedge(cx, cy - 10, 18, 13, 200, 340, 1); a_hline(cx - 15, cx + 15, cy - 10, 1); break;
    case M_GRIN:      a_arc(cx, cy - 10, 19, 200, 340, 3); a_hline(cx - 17, cx + 17, cy - 11, 2); break;
    case M_OPEN:      a_efill(cx, cy - 2, 10, 8, 1); a_efill(cx, cy + 2, 5, 3, 0); break;
    case M_SAD_OPEN:  a_efill(cx, cy + 2, 9, 8, 1); break;
    case M_FROWN:     a_arc(cx, cy + 8, 17, 25, 155, 2); break;
    case M_WAVY:      { int i; for (i = -14; i <= 14; i++) a_disc(cx + i, cy + (int)lroundf(3.0f * sinf((float)i / 3.0f)), 2, 1); } break;
    case M_ZIGZAG:
        a_seg(cx - 14, cy, cx - 5, cy - 6, 2); a_seg(cx - 5, cy - 6, cx + 5, cy + 5, 2);
        a_seg(cx + 5, cy + 5, cx + 14, cy - 1, 2);
        break;
    case M_O:         a_ring(cx, cy, 6, 3, 1); break;
    case M_SMIRK:     a_seg(cx - 12, cy + 3, cx + 2, cy + 5, 2); a_seg(cx + 2, cy + 5, cx + 11, cy - 3, 2); break;
    case M_TEETH:     a_hline(cx - 14, cx + 14, cy - 2, 2); a_hline(cx - 14, cx + 14, cy + 6, 2);
                      a_vline(cx - 5, cy - 2, cy + 6, 1); a_vline(cx + 5, cy - 2, cy + 6, 1); break;
    case M_TONGUE:    a_arc(cx, cy - 8, 16, 205, 335, 2); a_efill(cx, cy + 5, 7, 5, 1); break;
    case M_WHISTLE:   a_efill(cx, cy, 4, 4, 1); a_seg(cx + 6, cy + 1, cx + 11, cy + 1, 2); break;
    case M_POUT:      a_efill(cx, cy + 1, 8, 6, 1); break;
    case M_SCREAM:    a_efill(cx, cy, 11, 13, 1); a_efill(cx, cy + 6, 6, 4, 0); break;
    case M_ANGRY:                      /* ligne dure, coins abaisses */
        a_hline(cx - 14, cx + 14, cy, 2);
        a_seg(cx - 14, cy, cx - 20, cy + 6, 2);
        a_seg(cx + 14, cy, cx + 20, cy + 6, 2);
        break;
    case M_SNEER:                      /* coin releve + dents */
        a_seg(cx - 14, cy + 3, cx + 6, cy - 3, 2);
        a_seg(cx + 6, cy - 3, cx + 17, cy - 8, 2);
        a_hline(cx - 10, cx + 12, cy + 6, 1);
        break;
    default:          a_hline(cx - 15, cx + 15, cy, 2); break;
    }
}

void draw_bat_head(const expr_t *e)
{
    art_tilt((float)e->inclinaison, HX, HY);
    cowl(e->sourcil);
    eye(HX - 21, HY - 8, e->oeil_g, e->sourcil, 1);
    eye(HX + 21, HY - 8, e->oeil_d, e->sourcil_d, 0);
    mouth(e->bouche, HX, HY + 26);
    if (e->blush) {
        a_hline(HX - 25, HX - 18, HY + 20, 1);
        a_hline(HX + 18, HX + 25, HY + 20, 1);
    }
    art_no_tilt();
    if (e->symbole) a_symbole(e->symbole, e->sym_x, e->sym_y);
}

/* ---------------------------------------------------------------- */
/* familles emotionnelles + table d'expressions                     */
/* ---------------------------------------------------------------- */
const char *const FAMILLES[] = {
    "joie", "tristesse", "colere", "peur", "surprise", "degout", "interet",
    "reflexion", "calme", "fierte", "tendresse", "gene", "desir", "douleur"
};
const int FAMILLE_N = (int)(sizeof(FAMILLES) / sizeof(FAMILLES[0]));

/* nom, famille, oeil G, oeil D, sourcil G, sourcil D, bouche, inclinaison,
   symbole, x, y, rougeur
   Les 7 expressions que tout le monde reconnait a l'aveugle sont marquees
   par leur nom en tete de famille : colere, peur, surprise, degout, joie,
   tristesse + neutre (famille calme). */
#define E(nom, fam, og, od, sg, sd, mo, ti, sy, sx, syy, bl) \
    { nom, fam, og, od, sg, sd, mo, ti, sy, sx, syy, bl }

const expr_t EXPR[] = {
/* --- 0 JOIE (6) --- */
E("sourire_franc",      0, EYE_HAPPY,   EYE_HAPPY,   BROW_UP,      BROW_UP,      M_SMILE,     2, X_NONE,    0,  0, 1),
E("sourire_timide",     0, EYE_HALF,    EYE_HALF,    BROW_NEUTRAL, BROW_NEUTRAL, M_SMIRK,     3, X_NONE,    0,  0, 1),
E("rire_franc",         0, EYE_HAPPY,   EYE_HAPPY,   BROW_UP,      BROW_UP,      M_TEETH,     4, X_NONE,    0,  0, 1),
E("beatitude",          0, EYE_HAPPY,   EYE_HAPPY,   BROW_NEUTRAL, BROW_NEUTRAL, M_SMILE,     0, X_SPARK, 162, 48, 1),
E("sourire_narquois",   0, EYE_OPEN,    EYE_WINK,    BROW_UP,      BROW_NEUTRAL, M_SMIRK,     4, X_NONE,    0,  0, 1),
E("joie_exub",          0, EYE_STAR,    EYE_STAR,    BROW_UP,      BROW_UP,      M_SCREAM,   -3, X_SPARK,  38, 50, 1),
/* --- 1 TRISTESSE (7) --- */
E("tristesse",          1, EYE_OPEN,    EYE_OPEN,    BROW_SAD,     BROW_SAD,     M_FROWN,     6, X_DROP,  142, 62, 0),
E("chagrin",            1, EYE_TEARY,   EYE_TEARY,   BROW_SAD,     BROW_SAD,     M_SAD_OPEN,  7, X_DROP,  142, 64, 0),
E("melancolie",         1, EYE_LOOK_DOWN,EYE_LOOK_DOWN,BROW_SAD,   BROW_SAD,     M_FLAT,      8, X_NONE,    0,  0, 0),
E("deception",          1, EYE_HALF,    EYE_HALF,    BROW_SAD,     BROW_SAD,     M_POUT,      5, X_NONE,    0,  0, 0),
E("bouderie",           1, EYE_LOOK_L,  EYE_LOOK_L,  BROW_SAD,     BROW_SAD,     M_POUT,      6, X_NONE,    0,  0, 0),
E("desespoir",          1, EYE_LINE,    EYE_LINE,    BROW_SAD,     BROW_SAD,     M_SAD_OPEN, 12, X_DROP,  146, 66, 0),
E("regret",             1, EYE_NARROW,  EYE_NARROW,  BROW_SAD,     BROW_SAD,     M_ANGRY,     6, X_NONE,    0,  0, 0),
/* --- 2 COLERE (7) --- */
E("colere",             2, EYE_ANGRY,   EYE_ANGRY,   BROW_ANGRY,   BROW_ANGRY,   M_ANGRY,     0, X_ANGER, 162, 46, 0),
E("rage",               2, EYE_ANGRY,   EYE_ANGRY,   BROW_ANGRY_HARD,BROW_ANGRY_HARD,M_SCREAM, 0, X_VEIN,  156, 44, 0),
E("agacement",          2, EYE_HALF,    EYE_NARROW,  BROW_UP,      BROW_ANGRY,   M_SNEER,     3, X_NONE,    0,  0, 0),
E("frustration",        2, EYE_NARROW,  EYE_NARROW,  BROW_ANGRY,   BROW_ANGRY,   M_WAVY,      4, X_ANGER,  40, 52, 0),
E("defi",               2, EYE_OPEN,    EYE_OPEN,    BROW_ANGRY,   BROW_ANGRY,   M_SNEER,    -2, X_SPARK, 160, 52, 0),
E("grognement",         2, EYE_ANGRY,   EYE_ANGRY,   BROW_ANGRY,   BROW_ANGRY,   M_TEETH,     0, X_CROC,  152, 62, 0),
E("colere_contenue",    2, EYE_NARROW,  EYE_NARROW,  BROW_DET,     BROW_DET,     M_ANGRY,     2, X_NONE,    0,  0, 0),
/* --- 3 PEUR (6) --- */
E("peur",               3, EYE_TINY,    EYE_TINY,    BROW_SAD,     BROW_SAD,     M_WAVY,     -5, X_SWEAT, 148, 58, 0),
E("terreur",            3, EYE_TINY,    EYE_TINY,    BROW_UP,      BROW_UP,      M_SCREAM,  -10, X_MOTION,144, 56, 0),
E("inquietude",         3, EYE_LOOK_R,  EYE_LOOK_R,  BROW_SAD,     BROW_SAD,     M_FLAT,      4, X_SWEAT, 150, 62, 0),
E("anxiete",            3, EYE_WIDE,    EYE_WIDE,    BROW_SAD,     BROW_SAD,     M_WAVY,      5, X_SWEAT, 150, 60, 0),
E("tension",            3, EYE_NARROW,  EYE_NARROW,  BROW_DET,     BROW_DET,     M_TEETH,     3, X_NONE,    0,  0, 0),
E("effroi",             3, EYE_TINY,    EYE_TINY,    BROW_DET,     BROW_DET,     M_FLAT,     -7, X_SWEAT, 146, 52, 0),
/* --- 4 SURPRISE (5) --- */
E("surprise",           4, EYE_WIDE,    EYE_WIDE,    BROW_UP,      BROW_UP,      M_O,         0, X_NONE,    0,  0, 0),
E("etonnement",         4, EYE_WIDE,    EYE_WIDE,    BROW_UP,      BROW_UP,      M_SMIRK,     3, X_NONE,    0,  0, 0),
E("stupefaction",       4, EYE_WIDE,    EYE_WIDE,    BROW_UP,      BROW_UP,      M_OPEN,     -4, X_SPARK, 158, 46, 0),
E("sursaut",            4, EYE_LINE,    EYE_LINE,    BROW_UP,      BROW_UP,      M_OPEN,     -6, X_MOTION, 42, 52, 0),
E("incredulite",        4, EYE_NARROW,  EYE_WIDE,    BROW_UP,      BROW_NEUTRAL, M_SNEER,     5, X_QUESTION,156,50, 0),
/* --- 5 DEGOUT (5) --- */
E("degout",             5, EYE_ROLL,    EYE_ROLL,    BROW_ANGRY,   BROW_ANGRY,   M_WAVY,      6, X_DROP,  140, 74, 0),
E("mepris",             5, EYE_HALF,    EYE_NARROW,  BROW_NEUTRAL, BROW_ANGRY,   M_SNEER,     5, X_NONE,    0,  0, 0),
E("rejet",              5, EYE_LOOK_L,  EYE_LOOK_L,  BROW_SAD,     BROW_SAD,     M_POUT,      9, X_NONE,    0,  0, 0),
E("nausee",             5, EYE_ROLL,    EYE_LINE,    BROW_ANGRY,   BROW_ANGRY,   M_TONGUE,    8, X_SWEAT, 144, 72, 1),
E("scepticisme",        5, EYE_NARROW,  EYE_NARROW,  BROW_UP,      BROW_ANGRY,   M_SMIRK,     4, X_QUESTION,40,52, 0),
/* --- 6 INTERET (5) --- */
E("curiosite",          6, EYE_WIDE,    EYE_WIDE,    BROW_UP,      BROW_UP,      M_OPEN,      9, X_QUESTION,156,48, 0),
E("concentration",      6, EYE_NARROW,  EYE_NARROW,  BROW_DET,     BROW_DET,     M_FLAT,      0, X_NONE,    0,  0, 0),
E("vivacite",           6, EYE_OPEN,    EYE_OPEN,    BROW_UP,      BROW_UP,      M_SMILE,     3, X_SPARK, 160, 50, 0),
E("perplexite",         6, EYE_WIDE,    EYE_HALF,    BROW_UP,      BROW_SAD,     M_WAVY,      7, X_QUESTION,154,48, 0),
E("fascination",        6, EYE_WIDE,    EYE_WIDE,    BROW_UP,      BROW_UP,      M_O,         5, X_SPARK, 156, 46, 0),
/* --- 7 REFLEXION (4) --- */
E("reflexion",          7, EYE_LOOK_UP, EYE_LOOK_UP, BROW_NEUTRAL, BROW_NEUTRAL, M_FLAT,      6, X_DOTS,  158, 50, 0),
E("doute",              7, EYE_NARROW,  EYE_NARROW,  BROW_UP,      BROW_ANGRY,   M_SMIRK,     5, X_QUESTION,154,48, 0),
E("confusion",          7, EYE_WIDE,    EYE_NARROW,  BROW_SAD,     BROW_UP,      M_WAVY,      8, X_SPIRAL,156,48, 0),
E("indecision",         7, EYE_LOOK_DOWN,EYE_LOOK_DOWN,BROW_SAD,   BROW_SAD,     M_FLAT,      6, X_QUESTION,154,46, 0),
/* --- 8 CALME (6) : le noyau Batman --- */
E("neutre",             8, EYE_OPEN,    EYE_OPEN,    BROW_NEUTRAL, BROW_NEUTRAL, M_FLAT,      0, X_NONE,    0,  0, 0),
E("serieux",            8, EYE_NARROW,  EYE_NARROW,  BROW_DET,     BROW_DET,     M_FLAT,      0, X_NONE,    0,  0, 0),
E("sourire_en_coin",    8, EYE_HALF,    EYE_NARROW,  BROW_UP,      BROW_NEUTRAL, M_SMIRK,     4, X_NONE,    0,  0, 0),
E("determine",          8, EYE_NARROW,  EYE_NARROW,  BROW_DET,     BROW_DET,     M_ANGRY,     0, X_NONE,    0,  0, 0),
E("zen",                8, EYE_LINE,    EYE_LINE,    BROW_NEUTRAL, BROW_NEUTRAL, M_SMILE,     0, X_SPARK, 158, 52, 0),
E("blase",              8, EYE_HALF,    EYE_HALF,    BROW_SAD,     BROW_SAD,     M_SMIRK,     4, X_ZZZ,   152, 54, 0),
/* --- 9 FIERTE (5) --- */
E("fierte",             9, EYE_NARROW,  EYE_NARROW,  BROW_DET,     BROW_DET,     M_SMIRK,    -6, X_SPARK, 160, 50, 0),
E("assurance",          9, EYE_HALF,    EYE_HALF,    BROW_NEUTRAL, BROW_NEUTRAL, M_SMIRK,    -4, X_NONE,    0,  0, 0),
E("triomphe",           9, EYE_HAPPY,   EYE_HAPPY,   BROW_UP,      BROW_UP,      M_GRIN,     -5, X_SPARK,  40, 48, 0),
E("determination_heroique",9,EYE_NARROW,EYE_NARROW,  BROW_ANGRY,   BROW_ANGRY,   M_FLAT,     -3, X_NONE,    0,  0, 0),
E("arrogance",          9, EYE_HALF,    EYE_HALF,    BROW_UP,      BROW_NEUTRAL, M_SMIRK,    -8, X_NONE,    0,  0, 0),
/* --- 10 TENDRESSE (5) --- */
E("tendresse",         10, EYE_HAPPY,   EYE_HAPPY,   BROW_SAD,     BROW_SAD,     M_SMILE,     4, X_HEART, 160, 48, 1),
E("adoration",         10, EYE_HEART,   EYE_HEART,   BROW_UP,      BROW_UP,      M_SMILE,     5, X_HEART, 158, 44, 1),
E("bienveillance",     10, EYE_HAPPY,   EYE_HAPPY,   BROW_NEUTRAL, BROW_NEUTRAL, M_SMILE,     2, X_NONE,    0,  0, 1),
E("compassion",        10, EYE_HALF,    EYE_HALF,    BROW_SAD,     BROW_SAD,     M_SMILE,     5, X_DROP,  144, 62, 0),
E("soulagement",       10, EYE_HAPPY,   EYE_HAPPY,   BROW_UP,      BROW_UP,      M_OPEN,      0, X_SWEAT, 152, 56, 0),
/* --- 11 GENE (4) --- */
E("embarras",          11, EYE_LOOK_DOWN,EYE_LOOK_DOWN,BROW_SAD,   BROW_SAD,     M_WAVY,      6, X_SWEAT, 148, 60, 1),
E("honte",             11, EYE_LINE,    EYE_LINE,    BROW_SAD,     BROW_SAD,     M_FROWN,    12, X_DROP,  146, 64, 0),
E("timidite",          11, EYE_LOOK_R,  EYE_LOOK_R,  BROW_SAD,     BROW_SAD,     M_SMIRK,     7, X_NONE,    0,  0, 1),
E("malaise",           11, EYE_HALF,    EYE_HALF,    BROW_SAD,     BROW_SAD,     M_SMIRK,     5, X_HATCH, 152, 60, 0),
/* --- 12 DESIR (4) --- */
E("convoitise",        12, EYE_WIDE,    EYE_WIDE,    BROW_UP,      BROW_UP,      M_TONGUE,    4, X_SPARK, 158, 54, 0),
E("seduction",         12, EYE_HALF,    EYE_HALF,    BROW_ANGRY,   BROW_ANGRY,   M_SMIRK,     3, X_HEART, 158, 46, 0),
E("jalousie",          12, EYE_LOOK_R,  EYE_LOOK_R,  BROW_ANGRY,   BROW_ANGRY,   M_POUT,      4, X_NONE,    0,  0, 0),
E("vengeance",         12, EYE_ANGRY,   EYE_ANGRY,   BROW_ANGRY_HARD,BROW_ANGRY_HARD,M_SNEER, -2, X_VEIN,  158, 46, 0),
/* --- 13 DOULEUR (4) --- */
E("douleur",           13, EYE_TINY,    EYE_TINY,    BROW_ANGRY_HARD,BROW_ANGRY_HARD,M_TEETH, 0, X_PAIN, 158, 48, 0),
E("souffrance",        13, EYE_TEARY,   EYE_TEARY,   BROW_SAD,     BROW_SAD,     M_WAVY,      8, X_DROP,  144, 66, 0),
E("accablement",       13, EYE_LINE,    EYE_LINE,    BROW_SAD,     BROW_SAD,     M_FROWN,    13, X_HATCH, 150, 62, 0),
E("fatigue",           13, EYE_HALF,    EYE_HALF,    BROW_SAD,     BROW_SAD,     M_FLAT,      7, X_ZZZ,   152, 54, 0),
};

const int EXPR_N = (int)(sizeof(EXPR) / sizeof(EXPR[0]));

uint8_t FAMILLE_TAILLE[sizeof(FAMILLES) / sizeof(FAMILLES[0])];

static int g_tables_ok = 0;

void art_tables_init(void)
{
    int i;
    if (g_tables_ok) return;
    for (i = 0; i < FAMILLE_N; i++) FAMILLE_TAILLE[i] = 0;
    for (i = 0; i < EXPR_N; i++)
        if (EXPR[i].famille < FAMILLE_N) FAMILLE_TAILLE[EXPR[i].famille]++;
    g_tables_ok = 1;
}

int art_famille_id(const char *nom)
{
    int i;
    if (!nom) return -1;
    for (i = 0; i < FAMILLE_N; i++) if (strcmp(FAMILLES[i], nom) == 0) return i;
    return -1;
}

int art_expr_id(const char *nom)
{
    int i;
    if (!nom) return -1;
    for (i = 0; i < EXPR_N; i++) if (strcmp(EXPR[i].nom, nom) == 0) return i;
    return -1;
}

/* premiere expression d'une famille, decalee de k (pour varier les humeurs) */
int art_expr_de_famille(int famille, int k)
{
    int i, n = 0;
    art_tables_init();
    if (famille < 0 || famille >= FAMILLE_N) return 0;
    if (FAMILLE_TAILLE[famille] == 0) return 0;
    k %= FAMILLE_TAILLE[famille];
    if (k < 0) k += FAMILLE_TAILLE[famille];
    for (i = 0; i < EXPR_N; i++) {
        if (EXPR[i].famille != famille) continue;
        if (n == k) return i;
        n++;
    }
    return 0;
}

/* ================================================================== */
/* BATMino EN GRAND : cinq expressions qui se lisent de loin.          */
/* Trois indices par visage : le blanc des yeux, les sourcils blancs   */
/* sur la cagoule noire, et la bouche. Traits epais (5-7 px).          */
/* ================================================================== */
#define GCX 100
#define GEYE_Y 98
#define GRX 58
#define GRY 56

const char *const BAT5_NOM[] = { "CALME", "CONTENT", "EN COLERE", "TRISTE", "SURPRIS" };

/* un trait epais, dans la couleur demandee (1 = noir, 0 = blanc) */
static void barre(int x0, int y0, int x1, int y1, int t, int noir)
{
    int n = (abs(x1 - x0) > abs(y1 - y0)) ? abs(x1 - x0) : abs(y1 - y0);
    int i;
    if (n < 1) n = 1;
    for (i = 0; i <= n; i++) {
        int x = x0 + (x1 - x0) * i / n;
        int y = y0 + (y1 - y0) * i / n;
        a_disc(x, y, t, noir);
    }
}

static void grand_cagoule(int expression)
{
    int apex;

    /* oreilles : base large, pointe franche, bien dans le cadre */
    a_tri(GCX - 50, 48 - 12, GCX - 30, 14, GCX - 10, 48 - 30, 1);
    a_tri(GCX + 50, 48 - 12, GCX + 30, 14, GCX + 10, 48 - 30, 1);
    a_tri(GCX - 40, 48 - 16, GCX - 31, 24, GCX - 20, 48 - 30, 0);
    a_tri(GCX + 40, 48 - 16, GCX + 31, 24, GCX + 20, 48 - 30, 0);

    /* crane */
    a_efill(GCX, 104, GRX, GRY, 1);
    a_ering(GCX, 104, GRX, GRY, 4, 1);

    /* museau clair : la partie basse du visage */
    a_efill(GCX, 140, 42, 26, 0);
    a_ering(GCX, 140, 42, 26, 3, 1);

    /* arete du nez, pointe de la cagoule entre les yeux */
    apex = 132;
    if (expression == BAT5_FACHE) apex = 138;
    if (expression == BAT5_TRISTE) apex = 120;
    if (expression == BAT5_SURPRIS) apex = 108;
    a_tri(GCX - 22, 62, GCX + 22, 62, GCX, apex, 1);

    /* col de cape, deux pointes sous la machoire */
    a_tri(GCX - 40, 162, GCX - 70, 182, GCX - 12, 176, 1);
    a_tri(GCX + 40, 162, GCX + 70, 182, GCX + 12, 176, 1);
}

/* sourcils BLANCS sur la cagoule noire : c'est ce qui donne l'humeur */
static void grand_sourcil(int expression, int cx, int mirroir)
{
    float sg = mirroir ? -1.0f : 1.0f;
    int x0, y0, x1, y1;
    switch (expression) {
    case BAT5_CONTENT:                                   /* legerement releve */
        x0 = cx - (int)(20 * sg); y0 = GEYE_Y - 14 - 4;
        x1 = cx + (int)(16 * sg); y1 = GEYE_Y - 18 - 4;
        break;
    case BAT5_FACHE:                                     /* coin interieur bas */
        x0 = cx - (int)(22 * sg); y0 = GEYE_Y - 30;
        x1 = cx + (int)(14 * sg); y1 = GEYE_Y - 12;
        break;
    case BAT5_TRISTE:                                    /* coin interieur haut */
        x0 = cx - (int)(22 * sg); y0 = GEYE_Y - 12;
        x1 = cx + (int)(14 * sg); y1 = GEYE_Y - 30;
        break;
    case BAT5_SURPRIS:                                   /* haut et court */
        x0 = cx - (int)(16 * sg); y0 = GEYE_Y - 34;
        x1 = cx + (int)(16 * sg); y1 = GEYE_Y - 34;
        break;
    default:                                             /* neutre : droit */
        x0 = cx - (int)(20 * sg); y0 = GEYE_Y - 24;
        x1 = cx + (int)(16 * sg); y1 = GEYE_Y - 24;
        break;
    }
    barre(x0, y0, x1, y1, 5, 0);                         /* blanc */
}

static void grand_oeil(int expression, int cx, int mirroir, int larme)
{
    float sg = mirroir ? -1.0f : 1.0f;
    int rx = 24, ryu = 14, ryd = 12, pup = 10;
    float slope = 0.05f * sg;

    switch (expression) {
    case BAT5_CONTENT: ryu = 12; ryd = 17; slope = 0.14f * sg; pup = 11; break;
    case BAT5_FACHE:   ryu = 13; ryd = 8;  slope = 0.42f * sg; pup = 9;  break;
    case BAT5_TRISTE:  ryu = 16; ryd = 10; slope = -0.18f * sg; pup = 10; break;
    case BAT5_SURPRIS: ryu = 21; ryd = 19; slope = 0.0f; pup = 5; break;
    default: break;
    }
    almond(cx, GEYE_Y, rx, ryu, ryd, slope, 0, 0);
    a_arc(cx, GEYE_Y, rx, 195, 345, 4);                  /* contour de l'oeil */

    if (expression == BAT5_CONTENT) {                    /* oeil qui rit */
        a_hline(cx - rx + 3, cx + rx - 3, GEYE_Y + 1, 5);
        a_disc(cx + (int)(9 * sg), GEYE_Y + 4, 6, 1);
        return;
    }
    a_disc(cx + (int)(6 * sg), GEYE_Y + (int)(3 * sg), pup, 1);
    if (larme) {                                         /* larme qui coule */
        a_disc(cx + (int)(16 * sg), GEYE_Y + 22, 5, 1);
        a_tri(cx + (int)(11 * sg), GEYE_Y + 24, cx + (int)(21 * sg), GEYE_Y + 24,
              cx + (int)(16 * sg), GEYE_Y + 38, 1);
    }
}

static void grand_bouche(int expression)
{
    int cx = GCX, cy = 142;
    switch (expression) {
    case BAT5_CONTENT:                                   /* grand sourire ouvert */
        a_wedge(cx, cy - 16, 34, 26, 200, 340, 1);
        a_hline(cx - 30, cx + 30, cy - 16, 6);
        a_efill(cx, cy - 4, 22, 6, 0);
        break;
    case BAT5_FACHE:                                     /* ligne dure, coins bas */
        a_hline(cx - 30, cx + 30, cy, 7);
        barre(cx - 30, cy, cx - 42, cy + 14, 4, 1);
        barre(cx + 30, cy, cx + 42, cy + 14, 4, 1);
        break;
    case BAT5_TRISTE:                                    /* coins qui tombent */
        a_arc(cx, cy + 20, 32, 25, 155, 7);
        break;
    case BAT5_SURPRIS:                                   /* bouche ronde */
        a_ering(cx, cy + 2, 16, 20, 6, 1);
        break;
    default:                                             /* neutre : trait calme */
        a_hline(cx - 28, cx + 28, cy + 2, 6);
        break;
    }
}

void draw_bat_grand(int expression)
{
    if (expression < 0 || expression >= BAT5_N) expression = 0;
    grand_cagoule(expression);
    grand_sourcil(expression, GCX - 30, 1);
    grand_sourcil(expression, GCX + 30, 0);
    grand_oeil(expression, GCX - 30, 1, expression == BAT5_TRISTE);
    grand_oeil(expression, GCX + 30, 0, expression == BAT5_TRISTE);
    grand_bouche(expression);
}

/* ------------------------------------------------------------------ */
/* Un petit compagnon rond et tout doux (style Kirby) :                */
/* c'est lui qui represente BATMino sur l'ecran.                       */
/*   cx, cy : centre du corps, r : rayon (6..40), ph : etape du dessin */
/*   etat : 0 ravi, 1 tranquille, 2 triste/fatigue, 3 endormi          */
/* A chaque appel il est un peu different (il respire, il cligne,      */
/* il se dandine) : c'est ce qui le fait bouger d'un rafraichissement  */
/* a l'autre.                                                          */
/* ------------------------------------------------------------------ */
void draw_kirby(int cx, int cy, int r, int ph, int etat, float bouche)
{
    if (r < 8) r = 8;
    int petit = (r < 22);        /* tout petit : on ne garde que le corps et la frimousse */
    int bob = ((ph / 2) % 2) ? -2 : 0;           /* il respire : +-2 px */
    int cyb = cy + bob;
    int pas = (etat == 3) ? 0 : ((ph % 2) ? 2 : -2);   /* les pieds alternent, sauf s'il dort */
    int ex = r * 45 / 100;                       /* ecart des yeux */
    int ey = cyb - r / 4;

    if (!petit) {
        a_efill(cx - r / 2 + pas, cyb + r - r / 5, r / 3, r / 7, 1);    /* les pieds */
        a_efill(cx + r / 2 - pas, cyb + r - r / 5, r / 3, r / 7, 1);
        a_efill(cx - r + r / 6, cyb + r / 4, r / 4, r / 3, 1);          /* les bras */
        a_efill(cx + r - r / 6, cyb + r / 4, r / 4, r / 3, 1);
    }
    a_ering(cx, cyb, r, r, 2, 1);                /* le corps, rond */

    /* les yeux : deux points pleins en petit, deux ovales avec reflet en grand */
    if (bouche >= 0) {
        a_efill(cx - ex, ey, r / 7, r / 3, 1);
        a_efill(cx + ex, ey, r / 7, r / 3, 1);
        a_disc(cx - ex - r / 9, ey - r / 6, r / 11, 0);
        a_disc(cx + ex - r / 9, ey - r / 6, r / 11, 0);
    } else if (etat == 3 || (ph % 10 == 9)) {
        a_hline(cx - ex - r / 5, cx - ex + r / 5, ey, 2);      /* il ferme les yeux */
        a_hline(cx + ex - r / 5, cx + ex + r / 5, ey, 2);
    } else if (petit) {
        a_disc(cx - ex, ey, r / 5, 1);
        a_disc(cx + ex, ey, r / 5, 1);
    } else {
        a_efill(cx - ex, ey, r / 7, r / 3, 1);
        a_efill(cx + ex, ey, r / 7, r / 3, 1);
        a_disc(cx - ex - r / 9, ey - r / 6, r / 11, 0);        /* un reflet blanc */
        a_disc(cx + ex - r / 9, ey - r / 6, r / 11, 0);
    }

    /* la bouche : bien au-dessus du bas du corps, sinon elle se noie dedans */
    if (bouche >= 0) {
        int my = cyb + (petit ? r / 5 : r / 4);
        int hb = 1 + (int)(bouche * (float)r * 0.30f);      /* l'ouverture suit l'energie */
        int lb = r / 3;
        if (bouche < 0.06f) {
            a_hline(cx - lb, cx + lb, my, 2);
        } else {
            a_efill(cx, my, lb, hb, 1);
            if (hb > 4) a_hline(cx - lb + 2, cx + lb - 2, my + hb / 2, 2);   /* la langue */
        }
        return;
    }
    if (etat == 0)      a_arc(cx, cyb + (petit ? r / 5 : r / 4), r / 3, 205, 335, 2);
    else if (etat == 1) a_arc(cx, cyb + (petit ? r / 5 : r / 4), r / 3, 225, 315, 2);
    else if (etat == 3) {                                          /* il dort : une petite bouche ronde */
        if (petit) a_ering(cx, cyb + r / 4, r / 6, r / 6, 1, 1);
        else {
            a_text(cx + r / 2 + 2, cyb - r - 6, 2, "Z");
            a_text(cx + r / 2 + 16, cyb - r - 16, 2, "Z");
        }
    } else if (petit) {                                            /* triste : sourcils qui tombent + trait */
        a_seg(cx - ex + r / 6, ey - r / 2, cx - ex - r / 4, ey - r / 3, 1);
        a_seg(cx + ex - r / 6, ey - r / 2, cx + ex + r / 4, ey - r / 3, 1);
        a_hline(cx - r / 4, cx + r / 4, cyb + r / 3, 2);
    } else {
        a_arc(cx, cyb + r / 3, r / 4, 40, 140, 2);                 /* moue triste */
    }
}

/* ================================================================
 *  UI facon iOS : capsules, anneaux de progression, ecran TIMER.
 *  Ce code est partage par la carte et l'apercu PC : ce qu'on voit
 *  dans l'apercu est exactement ce que l'e-paper affiche.
 * ================================================================ */

void ui_capsule(int cx, int cy, int rx, int ry, int black)
{
    int plat = rx - ry;
    if (plat < 0) plat = 0;
    a_hline(cx - plat, cx + plat, cy - ry, 2 * ry + 1);   /* le corps (epaisseur vers le bas) */
    a_disc(cx - plat, cy, ry, black);                 /* les deux bouts arrondis */
    a_disc(cx + plat, cy, ry, black);
}

void ui_capsule_contour(int cx, int cy, int rx, int ry, int t)
{
    int plat = rx - ry;
    if (plat < 0) plat = 0;
    a_hline(cx - plat, cx + plat, cy - ry, t);
    a_hline(cx - plat, cx + plat, cy + ry, t);
    a_arc(cx - plat, cy, ry, 90, 270, t);
    a_arc(cx + plat, cy, ry, -90, 90, t);
}

void ui_anneau(int cx, int cy, int r, int pct, int t, int black)
{
    a_ring(cx, cy, r, 1, black);                     /* la piste, fine */
    if (pct > 0) {
        float a = (float)(pct > 100 ? 100 : pct) * 3.6f;
        a_arc_c(cx, cy, r, 90.0f - a, 90.0f, t, black);   /* depuis 12 h, sens horaire */
    }
}

/* ----------------------------------------------------------------
 *  L'ecran TIMER / ALARME : une liste propre, facon iOS.
 *  n      : nombre de lignes (0 = ecran vide avec un gros bouton +)
 *  sel    : ligne mise en avant (-1 = aucune)
 *  bas1/2 : les deux lignes d'aide en bas
 * ---------------------------------------------------------------- */
void ui_ecran_timer(const char *titre, const char *haut_d,
                    const char *const noms[], const char *const sous[],
                    const int pcts[], int n, int sel,
                    const char *bas1, const char *bas2)
{
    /* --- en-tete : titre a gauche, rappel a droite --- */
    a_hline(0, 199, 0, 3);
    a_hline(0, 199, 25, 3);
    a_text(6, 6, 2, titre);
    if (haut_d && haut_d[0]) {
        int w = a_text_w(1, haut_d);
        a_text(194 - w, 8, 1, haut_d);
    }

    /* --- aucune ligne : invitation claire --- */
    if (n <= 0) {
        a_ring(100, 74, 30, 2, 1);
        a_hline(84, 116, 74, 4);                     /* le + */
        a_vline(100, 58, 90, 4);
        a_text_c(100, 112, 2, "NOUVEAU TIMER");
        if (bas1) a_text_c(100, 136, 2, bas1);
        if (bas2) a_text_c(100, 152, 2, bas2);
        return;
    }

    /* --- la liste : jusqu'a 3 lignes --- */
    int max = n > 3 ? 3 : n;
    for (int i = 0; i < max; i++) {
        int cy = 50 + i * 42;
        int on = (i == sel);

        if (on) ui_capsule(100, cy, 95, 18, 1);      /* ligne active : capsule noire */
        else    ui_capsule_contour(100, cy, 95, 18, 1);

        /* le nom, en gras (taille 2) */
        a_text_col(14, cy - 12, 2, noms[i], on ? 0 : 1);
        /* le detail, en petit */
        if (sous && sous[i])
            a_text_col(14, cy + 2, 1, sous[i], on ? 0 : 1);
        /* l'etat a droite du texte */
        if (pcts && pcts[i] > 100)
            a_text_col(150, cy + 2, 1, "FINI", on ? 0 : 1);

        /* l'anneau de progression, a droite, facon Apple */
        if (pcts) {
            int p = pcts[i] > 100 ? 100 : pcts[i];
            ui_anneau(173, cy, 13, p, 2, on ? 0 : 1);
            char pc[16];
            snprintf(pc, sizeof(pc), "%d%%", p);
            a_text_col(173 - (a_text_w(1, pc) / 2), cy - 2, 1, pc, on ? 0 : 1);
        }
    }

    /* --- bas : le preset et les boutons, en corps 2 (lisible a bout de bras) --- */
    a_hline(0, 199, 166, 1);
    if (bas1) a_text_c(100, 172, 2, bas1);
    if (bas2) a_text_c(100, 186, 2, bas2);
}
