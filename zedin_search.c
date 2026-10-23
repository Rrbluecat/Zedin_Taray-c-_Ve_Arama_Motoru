#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#define INDEKS_DOSYASI "zedin_index.zidx"
#define KAYIT_LIMIT (2 * 1024 * 1024)
#define URL_LIMIT 8192
#define SORGU_LIMIT 512

typedef struct {
    char *url;
    char *metin;
    int puan;
} Sonuc;

static int oku(FILE *f, void *p, size_t n)
{
    return fread(p, 1, n, f) == n;
}

static int kelime_karakteri(unsigned char c)
{
    return isalnum(c) || c >= 128;
}

static int kelime_say(
    const char *metin, const char *kelime)
{
    int sayi = 0;
    size_t n = strlen(kelime);

    if (n == 0)
        return 0;

    const char *p = metin;

    while ((p = strcasestr(p, kelime)) != NULL) {
        int sol = (p == metin) ||
            !kelime_karakteri((unsigned char)p[-1]);

        int sag = !kelime_karakteri((unsigned char)p[n]);

        if (sol && sag)
            sayi++;

        p += n;
    }

    return sayi;
}

static void sorgu_kucult(char *s)
{
    for (; *s; s++)
        *s = (char)tolower((unsigned char)*s);
}

int main(void)
{
    FILE *f = fopen(INDEKS_DOSYASI, "rb");

    if (!f) {
        fprintf(stderr,
                "Indeks bulunamadi. Once ./zedin_indexer calistir.\n");
        return 1;
    }

    char imza[5];

    if (!oku(f, imza, 5) || memcmp(imza, "ZIDX1", 5) != 0) {
        fprintf(stderr, "Indeks dosyasi gecersiz.\n");
        fclose(f);
        return 1;
    }

    Sonuc sonuclar[256];
    int adet = 0;

    for (;;) {
        uint32_t url_n, metin_n;

        size_t okunan = fread(&url_n, sizeof url_n, 1, f);

        if (okunan == 0)
            break;

        if (!oku(f, &metin_n, sizeof metin_n) ||
            url_n == 0 || url_n >= URL_LIMIT ||
            metin_n == 0 || metin_n > KAYIT_LIMIT ||
            adet >= 256) {
            fprintf(stderr, "Indekste hatali kayit var.\n");
            fclose(f);
            return 1;
        }

        char *url = malloc((size_t)url_n + 1);
        char *metin = malloc((size_t)metin_n + 1);

        if (!url || !metin) {
            free(url);
            free(metin);
            fclose(f);
            return 1;
        }

        if (!oku(f, url, url_n) || !oku(f, metin, metin_n)) {
            free(url);
            free(metin);
            fprintf(stderr, "Indeks kaydi eksik.\n");
            fclose(f);
            return 1;
        }

        url[url_n] = '\0';
        metin[metin_n] = '\0';

        sonuclar[adet].url = url;
        sonuclar[adet].metin = metin;
        sonuclar[adet].puan = 0;
        adet++;
    }

    fclose(f);

    char sorgu[SORGU_LIMIT];

    printf("Zedin Search - Yerel arama\n");
    printf("Aranacak kelime: ");

    if (!fgets(sorgu, sizeof sorgu, stdin)) {
        for (int i = 0; i < adet; i++) {
            free(sonuclar[i].url);
            free(sonuclar[i].metin);
        }
        return 1;
    }

    sorgu[strcspn(sorgu, "\r\n")] = '\0';
    sorgu_kucult(sorgu);

    if (!sorgu[0]) {
        fprintf(stderr, "Arama sorgusu bos olamaz.\n");
        for (int i = 0; i < adet; i++) {
            free(sonuclar[i].url);
            free(sonuclar[i].metin);
        }
        return 1;
    }

    char *arama = strdup(sorgu);
    if (!arama)
        return 1;

    for (char *p = strtok(arama, " \t"); p;
         p = strtok(NULL, " \t")) {
        for (int i = 0; i < adet; i++)
            sonuclar[i].puan += kelime_say(sonuclar[i].metin, p);
    }

    free(arama);

    for (int i = 0; i < adet; i++) {
        for (int j = i + 1; j < adet; j++) {
            if (sonuclar[j].puan > sonuclar[i].puan) {
                Sonuc gecici = sonuclar[i];
                sonuclar[i] = sonuclar[j];
                sonuclar[j] = gecici;
            }
        }
    }

    printf("\nSonuclar:\n");

    int bulunan = 0;

    for (int i = 0; i < adet; i++) {
        if (sonuclar[i].puan <= 0)
            continue;

        printf("\n%d. %s\n", ++bulunan, sonuclar[i].url);
        printf("Eslesme puani: %d\n", sonuclar[i].puan);

        char *yer = strcasestr(sonuclar[i].metin, sorgu);

        if (yer) {
            printf("...%.*s...\n", 180, yer);
        }
    }

    if (!bulunan)
        printf("Eslesen sayfa bulunamadi.\n");

    for (int i = 0; i < adet; i++) {
        free(sonuclar[i].url);
        free(sonuclar[i].metin);
    }

    return 0;
}
