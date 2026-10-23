#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>

#include "chttp.h"

#define MAKS_SAYFA       200
#define MAKS_URL         4096
#define MAKS_HTML        (2U * 1024U * 1024U)
#define MAKS_KUYRUK      10000
#define MAKS_BILINEN     30000
#define BEKLE_MS         350

#define INDEKS_DOSYASI   "zedin_index.zidx"

/* Yalnızca bu hostlara istek gönderilecek. */
static const char *izinli_hostlar[] = {
    "tr.wikipedia.org",
    "tr.wiktionary.org",
    "tr.wikibooks.org",
    "tr.wikisource.org",
    "dergipark.org.tr",
    "developer.mozilla.org"
};

#define HOST_SAYISI \
    (sizeof(izinli_hostlar) / sizeof(izinli_hostlar[0]))

static char *kuyruk[MAKS_KUYRUK];
static char *bilinen[MAKS_BILINEN];

static size_t kuyruk_sayisi = 0;
static size_t kuyruk_konumu = 0;
static size_t bilinen_sayisi = 0;

static unsigned int indeks_sayisi = 0;


/* URL'nin host bölümünü alır. */
static int host_al(
    const char *url,
    char *host,
    size_t kapasite
)
{
    const char *baslangic;
    const char *son;
    size_t uzunluk;

    if (url == NULL || host == NULL || kapasite == 0)
        return 0;

    baslangic = strstr(url, "://");

    if (baslangic == NULL)
        return 0;

    baslangic += 3;
    son = baslangic;

    while (*son != '\0' &&
           *son != '/' &&
           *son != '?' &&
           *son != '#' &&
           *son != ':')
    {
        son++;
    }

    uzunluk = (size_t)(son - baslangic);

    if (uzunluk == 0 || uzunluk >= kapasite)
        return 0;

    memcpy(host, baslangic, uzunluk);
    host[uzunluk] = '\0';

    return 1;
}


/* Host, izin verilen kaynaklardan biri mi? */
static int host_izinli(const char *url)
{
    char host[256];

    if (!host_al(url, host, sizeof(host)))
        return 0;

    for (size_t i = 0; i < HOST_SAYISI; i++)
    {
        if (strcasecmp(host, izinli_hostlar[i]) == 0)
            return 1;
    }

    return 0;
}


/* URL'deki #sonrasını kaldırır. */
static void parcayi_temizle(char *url)
{
    char *parca = strchr(url, '#');

    if (parca != NULL)
        *parca = '\0';
}


/* URL daha önce kuyruğa eklenmiş mi? */
static int biliniyor_mu(const char *url)
{
    for (size_t i = 0; i < bilinen_sayisi; i++)
    {
        if (strcmp(bilinen[i], url) == 0)
            return 1;
    }

    return 0;
}


/* Bilinen URL listesine ekler. */
static int bilinen_ekle(const char *url)
{
    if (biliniyor_mu(url))
        return 1;

    if (bilinen_sayisi >= MAKS_BILINEN)
        return 0;

    char *kopya = strdup(url);

    if (kopya == NULL)
        return 0;

    bilinen[bilinen_sayisi++] = kopya;

    return 1;
}


/* URL'yi tarama kuyruğuna ekler. */
static int kuyruga_ekle(const char *url)
{
    if (!host_izinli(url))
        return 0;

    if (biliniyor_mu(url))
        return 1;

    if (kuyruk_sayisi >= MAKS_KUYRUK)
        return 0;

    if (bilinen_sayisi >= MAKS_BILINEN)
        return 0;

    char *kopya = strdup(url);

    if (kopya == NULL)
        return 0;

    if (!bilinen_ekle(url))
    {
        free(kopya);
        return 0;
    }

    kuyruk[kuyruk_sayisi++] = kopya;

    return 1;
}


/*
 * Bir bağlantıyı mevcut sayfanın adresine göre çözümler.
 *
 * Örnek:
 * /wiki/Linux       -> https://tr.wikipedia.org/wiki/Linux
 * makale.html       -> mevcut dizin + makale.html
 * //ornek.com/yol   -> https://ornek.com/yol
 */
static int url_coz(
    const char *taban,
    const char *baglanti,
    char *sonuc,
    size_t kapasite
)
{
    char gecici[MAKS_URL];
    char temel[MAKS_URL];

    const char *p;
    const char *govde;
    const char *son;

    size_t uzunluk;

    if (taban == NULL || baglanti == NULL ||
        sonuc == NULL || kapasite == 0)
        return 0;

    while (isspace((unsigned char)*baglanti))
        baglanti++;

    uzunluk = strlen(baglanti);

    while (uzunluk > 0 &&
           isspace((unsigned char)baglanti[uzunluk - 1]))
    {
        uzunluk--;
    }

    if (uzunluk == 0 || uzunluk >= sizeof(gecici))
        return 0;

    memcpy(gecici, baglanti, uzunluk);
    gecici[uzunluk] = '\0';

    /* Sayfa içi bağlantı. */
    if (gecici[0] == '#')
        return 0;

    /* İndekse dosya, betik veya başka şemalar ekleme. */
    if (strncasecmp(gecici, "javascript:", 11) == 0 ||
        strncasecmp(gecici, "mailto:", 7) == 0 ||
        strncasecmp(gecici, "data:", 5) == 0 ||
        strncasecmp(gecici, "tel:", 4) == 0)
    {
        return 0;
    }

    if (strncasecmp(gecici, "https://", 8) == 0 ||
        strncasecmp(gecici, "http://", 7) == 0)
    {
        if (strlen(gecici) >= kapasite)
            return 0;

        strcpy(sonuc, gecici);
        parcayi_temizle(sonuc);

        return host_izinli(sonuc);
    }

    p = strstr(taban, "://");

    if (p == NULL)
        return 0;

    govde = p + 3;
    son = govde;

    while (*son != '\0' &&
           *son != '/' &&
           *son != '?' &&
           *son != '#')
    {
        son++;
    }

    uzunluk = (size_t)(son - taban);

    if (uzunluk >= sizeof(temel))
        return 0;

    memcpy(temel, taban, uzunluk);
    temel[uzunluk] = '\0';

    /* //host/yol biçimi. */
    if (gecici[0] == '/' && gecici[1] == '/')
    {
        const char *sema = strstr(taban, "://");

        if (sema == NULL)
            return 0;

        uzunluk = (size_t)(sema - taban);

        if (uzunluk + strlen(gecici) + 1 >= kapasite)
            return 0;

        memcpy(sonuc, taban, uzunluk);
        sonuc[uzunluk] = '\0';

        strcat(sonuc, ":");
        strcat(sonuc, gecici);

        parcayi_temizle(sonuc);

        return host_izinli(sonuc);
    }

    /* /wiki/Linux gibi kökten başlayan yol. */
    if (gecici[0] == '/')
    {
        if (strlen(temel) + strlen(gecici) + 1 >= kapasite)
            return 0;

        strcpy(sonuc, temel);
        strcat(sonuc, gecici);
    }
    else
    {
        /*
         * Göreli bağlantı için mevcut sayfanın dizinini kullan.
         */
        char dizin[MAKS_URL];

        if (strlen(taban) >= sizeof(dizin))
            return 0;

        strcpy(dizin, taban);
        parcayi_temizle(dizin);

        char *soru = strchr(dizin, '?');

        if (soru != NULL)
            *soru = '\0';

        char *son_slash = strrchr(dizin, '/');

        if (son_slash == NULL ||
            son_slash < strstr(dizin, "://") + 3)
        {
            if (strlen(temel) + 1 >= sizeof(dizin))
                return 0;

            strcpy(dizin, temel);
            strcat(dizin, "/");
        }
        else
        {
            son_slash[1] = '\0';
        }

        if (strlen(dizin) + strlen(gecici) + 1 >= kapasite)
            return 0;

        strcpy(sonuc, dizin);
        strcat(sonuc, gecici);
    }

    parcayi_temizle(sonuc);

    return host_izinli(sonuc);
}



/* HTTP yonlendirmelerini en fazla 5 kez takip eder. */
static int yonlendirmeli_istek(
    const char *ilk_url,
    char *son_url,
    size_t kapasite,
    CHTTP_Response **yanit
)
{
    char aktif[MAKS_URL];

    if (ilk_url == NULL || son_url == NULL ||
        yanit == NULL || kapasite < 2 ||
        strlen(ilk_url) >= sizeof(aktif))
        return 0;

    strcpy(aktif, ilk_url);
    *yanit = NULL;

    for (int atlama = 0; atlama <= 5; atlama++)
    {
        if (!host_izinli(aktif))
            return 0;

        CHTTP_String adres = {
            .ptr = aktif,
            .len = (int)strlen(aktif)
        };

        int sonuc = chttp_get(adres, NULL, 0, yanit);

        if (sonuc != CHTTP_OK || *yanit == NULL)
        {
            if (*yanit != NULL)
            {
                chttp_free_response(*yanit);
                *yanit = NULL;
            }

            return 0;
        }

        int durum = (*yanit)->status;

        if (durum != 301 && durum != 302 &&
            durum != 303 && durum != 307 &&
            durum != 308)
        {
            if (strlen(aktif) >= kapasite)
            {
                chttp_free_response(*yanit);
                *yanit = NULL;
                return 0;
            }

            strcpy(son_url, aktif);
            return 1;
        }

        if (atlama == 5)
        {
            chttp_free_response(*yanit);
            *yanit = NULL;
            return 0;
        }

        CHTTP_String baslik_adi = {
            .ptr = "Location",
            .len = 8
        };

        int konum_indeksi = chttp_find_header(
            (*yanit)->headers,
            (*yanit)->num_headers,
            baslik_adi
        );

        if (konum_indeksi < 0)
        {
            chttp_free_response(*yanit);
            *yanit = NULL;
            return 0;
        }

        CHTTP_String konum_basligi =
            (*yanit)->headers[konum_indeksi].value;

        char konum[MAKS_URL];
        char sonraki[MAKS_URL];

        if (konum_basligi.ptr == NULL ||
            konum_basligi.len <= 0 ||
            (size_t)konum_basligi.len >= sizeof(konum))
        {
            chttp_free_response(*yanit);
            *yanit = NULL;
            return 0;
        }

        memcpy(
            konum,
            konum_basligi.ptr,
            (size_t)konum_basligi.len
        );

        konum[konum_basligi.len] = '\0';

        chttp_free_response(*yanit);
        *yanit = NULL;

        if (!url_coz(
                aktif,
                konum,
                sonraki,
                sizeof(sonraki)))
        {
            return 0;
        }

        if (strlen(sonraki) >= sizeof(aktif))
            return 0;

        strcpy(aktif, sonraki);
    }

    return 0;
}

/* HTML içindeki href değerlerini bulup kuyruğa ekler. */
static void baglantilari_tara(
    const char *html,
    const char *sayfa_url
)
{
    const char *p = html;

    while (*p != '\0' &&
           kuyruk_sayisi < MAKS_KUYRUK &&
           indeks_sayisi < MAKS_SAYFA)
    {
        if (strncasecmp(p, "href", 4) != 0)
        {
            p++;
            continue;
        }

        /*
         * data-href gibi başka özniteliklerin içindeki
         * href parçalarını bağlantı sanma.
         */
        if (p > html &&
            (isalnum((unsigned char)p[-1]) ||
             p[-1] == '-' ||
             p[-1] == '_'))
        {
            p++;
            continue;
        }

        const char *q = p + 4;

        while (isspace((unsigned char)*q))
            q++;

        if (*q != '=')
        {
            p++;
            continue;
        }

        q++;

        while (isspace((unsigned char)*q))
            q++;

        char tirnak = '\0';

        if (*q == '"' || *q == '\'')
            tirnak = *q++;

        const char *baslangic = q;

        if (tirnak != '\0')
        {
            while (*q != '\0' && *q != tirnak)
                q++;
        }
        else
        {
            while (*q != '\0' &&
                   !isspace((unsigned char)*q) &&
                   *q != '>')
            {
                q++;
            }
        }

        size_t uzunluk = (size_t)(q - baslangic);

        if (uzunluk > 0 && uzunluk < MAKS_URL)
        {
            char baglanti[MAKS_URL];
            char tam_url[MAKS_URL];

            memcpy(baglanti, baslangic, uzunluk);
            baglanti[uzunluk] = '\0';

            if (url_coz(
                    sayfa_url,
                    baglanti,
                    tam_url,
                    sizeof(tam_url)))
            {
                kuyruga_ekle(tam_url);
            }
        }

        p = q;

        if (*p != '\0')
            p++;
    }
}


/* HTML etiketlerini çıkarıp okunabilir metni oluşturur. */
static char *html_metni(
    const char *html,
    size_t uzunluk,
    size_t *metin_uzunlugu
)
{
    char *metin = malloc(uzunluk + 1);

    if (metin == NULL)
        return NULL;

    size_t cikis = 0;

    int etiket = 0;
    int betik = 0;
    int stil = 0;
    int bosluk = 1;

    for (size_t i = 0; i < uzunluk; i++)
    {
        if (html[i] == '<')
        {
            size_t kalan = uzunluk - i;

            if (kalan >= 7 &&
                strncasecmp(html + i, "<script", 7) == 0)
            {
                betik = 1;
            }
            else if (kalan >= 8 &&
                     strncasecmp(html + i, "</script", 8) == 0)
            {
                betik = 0;
            }
            else if (kalan >= 6 &&
                     strncasecmp(html + i, "<style", 6) == 0)
            {
                stil = 1;
            }
            else if (kalan >= 7 &&
                     strncasecmp(html + i, "</style", 7) == 0)
            {
                stil = 0;
            }

            etiket = 1;
            continue;
        }

        if (html[i] == '>')
        {
            etiket = 0;
            continue;
        }

        if (etiket || betik || stil)
            continue;

        unsigned char c = (unsigned char)html[i];

        if (isspace(c))
        {
            if (!bosluk && cikis > 0)
            {
                metin[cikis++] = ' ';
                bosluk = 1;
            }
        }
        else
        {
            metin[cikis++] = (char)c;
            bosluk = 0;
        }
    }

    while (cikis > 0 && metin[cikis - 1] == ' ')
        cikis--;

    metin[cikis] = '\0';

    *metin_uzunlugu = cikis;

    return metin;
}


/* URL ve metni mevcut ZIDX1 biçiminde kaydeder. */
static int indeks_kaydet(
    FILE *dosya,
    const char *url,
    const char *metin,
    size_t metin_uzunlugu
)
{
    size_t url_uzunlugu = strlen(url);

    if (url_uzunlugu == 0 ||
        url_uzunlugu > UINT32_MAX ||
        metin_uzunlugu > UINT32_MAX)
    {
        return 0;
    }

    uint32_t url_n = (uint32_t)url_uzunlugu;
    uint32_t metin_n = (uint32_t)metin_uzunlugu;

    if (fwrite(&url_n, sizeof(url_n), 1, dosya) != 1 ||
        fwrite(&metin_n, sizeof(metin_n), 1, dosya) != 1 ||
        fwrite(url, 1, url_uzunlugu, dosya) != url_uzunlugu ||
        fwrite(metin, 1, metin_uzunlugu, dosya) != metin_uzunlugu)
    {
        return 0;
    }

    return 1;
}


/*
 * Önceki indeks kayıtlarını yükler.
 * Böylece sonraki çalıştırmalarda aynı URL'leri
 * tekrar indekslemeye çalışmayız.
 */
static int mevcut_indeksi_yukle(void)
{
    FILE *dosya = fopen(INDEKS_DOSYASI, "rb");

    if (dosya == NULL)
    {
        if (errno == ENOENT)
            return 1;

        perror("Mevcut indeks açılamadı");
        return 0;
    }

    char magic[5];

    if (fread(magic, 1, 5, dosya) != 5 ||
        memcmp(magic, "ZIDX1", 5) != 0)
    {
        fprintf(stderr, "Hata: İndeks biçimi tanınmıyor.\n");
        fclose(dosya);
        return 0;
    }

    for (;;)
    {
        uint32_t url_n;
        uint32_t metin_n;

        if (fread(&url_n, sizeof(url_n), 1, dosya) != 1)
        {
            if (feof(dosya))
                break;

            fclose(dosya);
            return 0;
        }

        if (fread(&metin_n, sizeof(metin_n), 1, dosya) != 1 ||
            url_n == 0 ||
            url_n >= MAKS_URL ||
            metin_n > MAKS_HTML)
        {
            fprintf(stderr, "Hata: İndeks kaydı bozuk.\n");
            fclose(dosya);
            return 0;
        }

        char *url = malloc((size_t)url_n + 1);
        char *metin = malloc((size_t)metin_n + 1);

        if (url == NULL || metin == NULL)
        {
            free(url);
            free(metin);
            fclose(dosya);
            return 0;
        }

        if (fread(url, 1, url_n, dosya) != url_n ||
            fread(metin, 1, metin_n, dosya) != metin_n)
        {
            free(url);
            free(metin);
            fclose(dosya);
            fprintf(stderr, "Hata: İndeks dosyası eksik.\n");
            return 0;
        }

        url[url_n] = '\0';
        metin[metin_n] = '\0';

        if (!bilinen_ekle(url))
        {
            free(url);
            free(metin);
            fclose(dosya);
            return 0;
        }

        indeks_sayisi++;

        free(url);
        free(metin);
    }

    fclose(dosya);

    printf("Mevcut indeks kaydı: %u\n", indeks_sayisi);

    return 1;
}


/* İstekler arasında kısa bir bekleme uygular. */
static void bekle(void)
{
    struct timespec sure;

    sure.tv_sec = BEKLE_MS / 1000;
    sure.tv_nsec = (long)(BEKLE_MS % 1000) * 1000000L;

    nanosleep(&sure, NULL);
}


int main(void)
{
    static const char *baslangiclar[] = {
        "https://tr.wikipedia.org/",
        "https://tr.wiktionary.org/",
        "https://tr.wikibooks.org/",
        "https://tr.wikisource.org/",
        "https://dergipark.org.tr/",
        "https://developer.mozilla.org/tr/"
    };

    printf("Zedin Indexer - Yerel Web Indeksi\n\n");

    if (!mevcut_indeksi_yukle())
        return 1;

    if (indeks_sayisi >= MAKS_SAYFA)
    {
        printf("Indeks zaten %u sayfa sinirina ulasmis.\n",
               MAKS_SAYFA);

        return 0;
    }

    for (size_t i = 0;
         i < sizeof(baslangiclar) / sizeof(baslangiclar[0]);
         i++)
    {
        kuyruga_ekle(baslangiclar[i]);
    }

    FILE *dosya = fopen(INDEKS_DOSYASI, "ab");

    if (dosya == NULL)
    {
        perror("Indeks dosyasi acilamadi");
        return 1;
    }

    /* Yeni dosyaysa biçim başlığını yaz. */
    if (indeks_sayisi == 0)
    {
        FILE *kontrol = fopen(INDEKS_DOSYASI, "rb");

        if (kontrol == NULL)
        {
            fclose(dosya);
            return 1;
        }

        char magic[5];
        size_t okunan = fread(magic, 1, 5, kontrol);

        fclose(kontrol);

        if (okunan == 0)
        {
            if (fwrite("ZIDX1", 1, 5, dosya) != 5)
            {
                fclose(dosya);
                return 1;
            }
        }
    }

    while (kuyruk_konumu < kuyruk_sayisi &&
           indeks_sayisi < MAKS_SAYFA)
    {
        char *url = kuyruk[kuyruk_konumu++];

        printf("[%u/%u] %s\n",
               indeks_sayisi,
               MAKS_SAYFA,
               url);

        char son_url[MAKS_URL];
        CHTTP_Response *yanit = NULL;

        if (!yonlendirmeli_istek(
                url,
                son_url,
                sizeof(son_url),
                &yanit))
        {
            printf("  Istek veya yonlendirme basarisiz; atlaniyor.\\n");
            bekle();
            continue;
        }

        if (yanit->status < 200 || yanit->status >= 300 ||
            yanit->body.ptr == NULL ||
            yanit->body.len <= 0 ||
            (size_t)yanit->body.len > MAKS_HTML)
        {
            printf("  HTTP %d veya gecersiz sayfa; atlaniyor.\n",
                   yanit->status);

            chttp_free_response(yanit);
            bekle();
            continue;
        }

        size_t html_uzunlugu = (size_t)yanit->body.len;

        char *html = malloc(html_uzunlugu + 1);

        if (html == NULL)
        {
            chttp_free_response(yanit);
            break;
        }

        memcpy(html, yanit->body.ptr, html_uzunlugu);
        html[html_uzunlugu] = '\0';

        chttp_free_response(yanit);

        /*
         * Önce bağlantıları keşfet.
         * Kuyruk yeni URL'lerle genişler.
         */
        baglantilari_tara(html, son_url);

        size_t metin_uzunlugu = 0;

        char *metin = html_metni(
            html,
            html_uzunlugu,
            &metin_uzunlugu
        );

        free(html);

        if (metin == NULL)
        {
            printf("  Bellek yetersiz; atlaniyor.\n");
            bekle();
            continue;
        }

        /*
         * Boş veya çok kısa sayfaları indeksleme.
         */
        if (metin_uzunlugu < 40)
        {
            printf("  Yeterli metin yok; atlaniyor.\n");
            free(metin);
            bekle();
            continue;
        }

        if (!indeks_kaydet(dosya, son_url, metin, metin_uzunlugu))
        {
            fprintf(stderr, "Indeks yazma hatasi.\n");
            free(metin);
            fclose(dosya);
            return 1;
        }

        free(metin);

        indeks_sayisi++;

        if (fflush(dosya) != 0)
        {
            perror("Indeks diske yazilamadi");
            fclose(dosya);
            return 1;
        }

        printf("  Kaydedildi. Toplam: %u | Kuyruk: %zu\n",
               indeks_sayisi,
               kuyruk_sayisi - kuyruk_konumu);

        bekle();
    }

    fclose(dosya);

    printf("\nTarama tamamlandi.\n");
    printf("Toplam indeks kaydi: %u\n", indeks_sayisi);
    printf("Bu oturumda bulunan URL: %zu\n", bilinen_sayisi);
    printf("Indeks dosyasi: %s\n", INDEKS_DOSYASI);

    for (size_t i = 0; i < kuyruk_sayisi; i++)
        free(kuyruk[i]);

    for (size_t i = 0; i < bilinen_sayisi; i++)
        free(bilinen[i]);

    return 0;
}
