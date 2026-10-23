#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "html_entity.h"

struct varlik {
    const char *ad;
    long kod;
};

static const struct varlik varliklar[] = {
    {"amp", '&'}, {"lt", '<'}, {"gt", '>'}, {"quot", '"'}, {"apos", '\''},
    {"ldquo", 0x201C}, {"rdquo", 0x201D},
    {"lsquo", 0x2018}, {"rsquo", 0x2019},
    {"ndash", 0x2013}, {"mdash", 0x2014},
    {"hellip", 0x2026}, {"copy", 0xA9}, {"reg", 0xAE},
    {NULL, 0}
};

static void utf8_yaz(long n)
{
    if (n < 0x80) {
        putchar((int)n);
    } else if (n < 0x800) {
        putchar(0xC0 | (n >> 6));
        putchar(0x80 | (n & 0x3F));
    } else if (n < 0x10000) {
        putchar(0xE0 | (n >> 12));
        putchar(0x80 | ((n >> 6) & 0x3F));
        putchar(0x80 | (n & 0x3F));
    } else if (n <= 0x10FFFF) {
        putchar(0xF0 | (n >> 18));
        putchar(0x80 | ((n >> 12) & 0x3F));
        putchar(0x80 | ((n >> 6) & 0x3F));
        putchar(0x80 | (n & 0x3F));
    }
}

int html_entity_yaz(const char *html, int i, int uzunluk, int *onceki_bosluk)
{
    int j;
    int son = -1;

    /* En fazla 10 karakter ileride ';' ara */
    for (j = i + 1; j < uzunluk && j <= i + 10; j++) {
        if (html[j] == ';') { son = j; break; }
    }
    if (son < 0) {
        putchar('&');
        *onceki_bosluk = 0;
        return i;
    }

    const char *ad = html + i + 1;
    int len = son - (i + 1);
    long kod = -1;

    if (len == 4 && strncmp(ad, "nbsp", 4) == 0) {
        if (!*onceki_bosluk) { putchar(' '); *onceki_bosluk = 1; }
        return son;
    }

    if (len >= 2 && ad[0] == '#') {
        char tmp[12];
        if (len - 1 < (int)sizeof(tmp)) {
            memcpy(tmp, ad + 1, len - 1);
            tmp[len - 1] = '\0';
            if (tmp[0] == 'x' || tmp[0] == 'X')
                kod = strtol(tmp + 1, NULL, 16);
            else
                kod = strtol(tmp, NULL, 10);
        }
    } else {
        for (int k = 0; varliklar[k].ad; k++) {
            if ((int)strlen(varliklar[k].ad) == len &&
                strncmp(ad, varliklar[k].ad, len) == 0) {
                kod = varliklar[k].kod;
                break;
            }
        }
    }

    if (kod > 0)
        utf8_yaz(kod);
    *onceki_bosluk = 0;
    return son;
}
