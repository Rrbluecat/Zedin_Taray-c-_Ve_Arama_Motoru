#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <ctype.h>
#include "chttp.h"

#define SURUM "0.2"
#define URL_MAX 1024
#define GECMIS_MAX 32
#define MAX_BAGLANTI 512

#define ZEMIN "\033[48;2;16;24;39m"
#define YAZI  "\033[38;2;226;232;240m"
#define MAVI  "\033[1;38;2;96;165;250m"
#define GRI   "\033[38;2;148;163;184m"
#define SIFIR "\033[0m" ZEMIN
#define BAR   "\033[48;2;37;51;70m\033[38;2;203;213;225m"

enum { TUS_YOK = -1, TUS_KESME = -2, TUS_HATA = -3,
       TUS_YUKARI = 1001, TUS_ASAGI, TUS_SAYFA_YUKARI, TUS_SAYFA_ASAGI };

/* ================= TAMPON ================= */

typedef struct { char *veri; size_t uzunluk; size_t kapasite; } Tampon;

static void tampon_baslat(Tampon *t)
{ t->veri = NULL; t->uzunluk = 0; t->kapasite = 0; }

static void tampon_temizle(Tampon *t)
{ free(t->veri); tampon_baslat(t); }

static int tampon_yaz(Tampon *t, const char *s, size_t n)
{
    if (t->uzunluk + n + 1 > t->kapasite) {
        size_t yeni = t->kapasite ? t->kapasite : 256;
        while (yeni < t->uzunluk + n + 1) yeni *= 2;
        char *g = realloc(t->veri, yeni);
        if (!g) return -1;
        t->veri = g;
        t->kapasite = yeni;
    }
    memcpy(t->veri + t->uzunluk, s, n);
    t->uzunluk += n;
    t->veri[t->uzunluk] = '\0';
    return 0;
}

static int tampon_ekle(Tampon *t, char c)
{ return tampon_yaz(t, &c, 1); }

/* ================= HTML VARLIKLARI ================= */

struct varlik { const char *ad; long kod; };

static const struct varlik varliklar[] = {
    {"amp", '&'}, {"lt", '<'}, {"gt", '>'}, {"quot", '"'}, {"apos", '\''},
    {"ldquo", 0x201C}, {"rdquo", 0x201D}, {"lsquo", 0x2018}, {"rsquo", 0x2019},
    {"ndash", 0x2013}, {"mdash", 0x2014}, {"hellip", 0x2026},
    {"copy", 0xA9}, {"reg", 0xAE}, {NULL, 0}
};

static void utf8_yaz(Tampon *t, long n)
{
    char b[4];
    int k = 0;
    if (n < 0x80) {
        b[k++] = (char)n;
    } else if (n < 0x800) {
        b[k++] = (char)(0xC0 | (n >> 6));
        b[k++] = (char)(0x80 | (n & 0x3F));
    } else if (n < 0x10000) {
        b[k++] = (char)(0xE0 | (n >> 12));
        b[k++] = (char)(0x80 | ((n >> 6) & 0x3F));
        b[k++] = (char)(0x80 | (n & 0x3F));
    } else if (n <= 0x10FFFF) {
        b[k++] = (char)(0xF0 | (n >> 18));
        b[k++] = (char)(0x80 | ((n >> 12) & 0x3F));
        b[k++] = (char)(0x80 | ((n >> 6) & 0x3F));
        b[k++] = (char)(0x80 | (n & 0x3F));
    }
    if (k > 0) tampon_yaz(t, b, (size_t)k);
}

static int gui_varlik_yaz(const char *html, int i, int uzunluk,
                           int *onceki_bosluk, Tampon *cikti)
{
    int j, son = -1;
    for (j = i + 1; j < uzunluk && j <= i + 10; j++)
        if (html[j] == ';') { son = j; break; }

    if (son < 0) {
        tampon_ekle(cikti, '&');
        *onceki_bosluk = 0;
        return i;
    }

    const char *ad = html + i + 1;
    int len = son - (i + 1);
    long kod = -1;

    if (len == 4 && strncmp(ad, "nbsp", 4) == 0) {
        if (!*onceki_bosluk) { tampon_ekle(cikti, ' '); *onceki_bosluk = 1; }
        return son;
    }

    if (len >= 2 && ad[0] == '#') {
        char tmp[12];
        if (len - 1 < (int)sizeof(tmp)) {
            memcpy(tmp, ad + 1, (size_t)(len - 1));
            tmp[len - 1] = '\0';
            if (tmp[0] == 'x' || tmp[0] == 'X') kod = strtol(tmp + 1, NULL, 16);
            else                                kod = strtol(tmp, NULL, 10);
        }
    } else {
        for (int k = 0; varliklar[k].ad; k++) {
            if ((int)strlen(varliklar[k].ad) == len &&
                strncmp(ad, varliklar[k].ad, (size_t)len) == 0) {
                kod = varliklar[k].kod;
                break;
            }
        }
    }

    if (kod > 0) utf8_yaz(cikti, kod);
    *onceki_bosluk = 0;
    return son;
}

/* ================= HTML -> METIN ================= */

static int oneki(const char *p, int kalan, const char *s)
{
    int n = (int)strlen(s);
    return kalan >= n && strncasecmp(p, s, (size_t)n) == 0;
}

static int blok_etiketi(const char *p, int kalan)
{
    static const char *adlar[] = {"p","br","div","li","ul","ol","h1","h2","h3",
        "h4","h5","h6","tr","table","section","article","header","footer",
        "nav","title", NULL};
    if (kalan < 2) return 0;
    p++; kalan--;
    if (*p == '/') { p++; kalan--; }
    for (int k = 0; adlar[k]; k++) {
        int n = (int)strlen(adlar[k]);
        if (kalan > n && strncasecmp(p, adlar[k], (size_t)n) == 0) {
            char s = p[n];
            if (s=='>' || s==' ' || s=='/' || s=='\t' || s=='\n' || s=='\r')
                return 1;
        }
    }
    return 0;
}

static void satir_sonu(Tampon *t, int *onceki_bosluk)
{
    if (t->uzunluk > 0) {
        char *u = &t->veri[t->uzunluk - 1];
        if (*u == ' ') *u = '\n';
        else if (*u != '\n') tampon_ekle(t, '\n');
    }
    *onceki_bosluk = 1;
}

static void html_metne_cevir(const char *html, int uzunluk, Tampon *cikti)
{
    int etiket_icinde = 0, script_icinde = 0, style_icinde = 0;
    int onceki_bosluk = 1;

    for (int i = 0; i < uzunluk; i++) {
        char karakter = html[i];
        if (karakter == '<') {
            int kalan = uzunluk - i;
            if (oneki(html + i, kalan, "<script"))       script_icinde = 1;
            else if (oneki(html + i, kalan, "</script")) script_icinde = 0;
            if (oneki(html + i, kalan, "<style"))        style_icinde = 1;
            else if (oneki(html + i, kalan, "</style"))  style_icinde = 0;
            if (blok_etiketi(html + i, kalan)) satir_sonu(cikti, &onceki_bosluk);
            etiket_icinde = 1;
        } else if (karakter == '>') {
            etiket_icinde = 0;
        } else if (!etiket_icinde && !script_icinde && !style_icinde) {
            if (karakter == ' ' || karakter == '\t' ||
                karakter == '\n' || karakter == '\r') {
                if (!onceki_bosluk) {
                    tampon_ekle(cikti, ' ');
                    onceki_bosluk = 1;
                }
            } else if (karakter == '&') {
                i = gui_varlik_yaz(html, i, uzunluk, &onceki_bosluk, cikti);
            } else {
                tampon_ekle(cikti, karakter);
                onceki_bosluk = 0;
            }
        }
    }
}

/* ================= BAGLANTILAR ================= */

typedef struct { char *adres[MAX_BAGLANTI]; int sayi; } BaglantiListesi;

static int a_etiketi_mi(const char *html, int i)
{
    int j = i;
    while (j > 0 && html[j] != '<' && i - j < 500) j--;
    if (html[j] != '<') return 0;
    return (html[j + 1] == 'a' || html[j + 1] == 'A') &&
           j + 2 < i && isspace((unsigned char)html[j + 2]);
}

static void baglantilari_topla(const char *html, int uzunluk, BaglantiListesi *l)
{
    l->sayi = 0;
    for (int i = 0; i + 5 < uzunluk && l->sayi < MAX_BAGLANTI; i++) {
        if (strncasecmp(html + i, "href=", 5) != 0) continue;
        if (i > 0 && !isspace((unsigned char)html[i - 1])) continue;
        if (!a_etiketi_mi(html, i)) continue;

        int bas = i + 5;
        char t = html[bas];
        if (t != '"' && t != '\'') continue;
        bas++;
        int son = bas;
        while (son < uzunluk && html[son] != t) son++;
        if (son >= uzunluk) break;

        int n = son - bas;
        if (n > 0 && html[bas] != '#' &&
            !oneki(html + bas, n, "javascript:") &&
            !oneki(html + bas, n, "mailto:")) {
            char *kopya = malloc((size_t)n + 1);
            if (!kopya) break;
            int w = 0;
            for (int k = 0; k < n; k++) {
                if (html[bas + k] == '&' && k + 5 <= n &&
                    strncmp(html + bas + k, "&amp;", 5) == 0) {
                    kopya[w++] = '&';
                    k += 4;
                } else {
                    kopya[w++] = html[bas + k];
                }
            }
            kopya[w] = '\0';
            l->adres[l->sayi++] = kopya;
        }
        i = son;
    }
}

static void baglantilari_temizle(BaglantiListesi *l)
{
    for (int i = 0; i < l->sayi; i++) free(l->adres[i]);
    l->sayi = 0;
}

/* ================= DURUM ================= */

static volatile sig_atomic_t yeniden_ciz = 1;
static volatile sig_atomic_t calisiyor = 1;
static struct termios eski_terminal;
static int terminal_ayari = 0;

static Tampon metin;
static BaglantiListesi baglantilar;
static char simdiki_url[URL_MAX] = "";
static char mesaj[256] = "Adres girmek icin g tusuna bas";
static int durum_kodu = 0;
static int kaydirma = 0, bag_kaydirma = 0;
static int mod = 0;                 /* 0 = sayfa, 1 = baglanti listesi */

static size_t *satir_baslari = NULL;
static int satir_adet = 0, satir_kap = 0, hesap_genislik = 0;

static char gecmis[GECMIS_MAX][URL_MAX];
static int gecmis_sayi = 0, gecmis_konum = -1;

/* ================= TERMINAL ================= */

static void boyut_degisti(int s) { (void)s; yeniden_ciz = 1; }
static void cik(int s)           { (void)s; calisiyor = 0; }

static void terminali_sifirla(void)
{
    if (terminal_ayari) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &eski_terminal);
        terminal_ayari = 0;
    }
    printf("\033[0m\033[?25h\033[?1049l");
    fflush(stdout);
}

static void boyut_al(int *satir, int *sutun)
{
    struct winsize ws;
    *satir = 24; *sutun = 80;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        if (ws.ws_row > 0) *satir = ws.ws_row;
        if (ws.ws_col > 0) *sutun = ws.ws_col;
    }
    if (*satir < 8)  *satir = 8;
    if (*sutun < 20) *sutun = 20;
}

static int tus_oku(void)
{
    unsigned char c, s;
    ssize_t n = read(STDIN_FILENO, &c, 1);
    if (n < 0) return errno == EINTR ? TUS_KESME : TUS_HATA;
    if (n == 0) return TUS_YOK;
    if (c == 27) {
        if (read(STDIN_FILENO, &s, 1) == 1 && s == '[' &&
            read(STDIN_FILENO, &s, 1) == 1) {
            if (s == 'A') return TUS_YUKARI;
            if (s == 'B') return TUS_ASAGI;
            if (s == '5') { (void)read(STDIN_FILENO, &s, 1); return TUS_SAYFA_YUKARI; }
            if (s == '6') { (void)read(STDIN_FILENO, &s, 1); return TUS_SAYFA_ASAGI; }
        }
        return 27;
    }
    return c;
}

/* ================= SATIRLARA BOLME ================= */

static int satir_ekle(size_t b)
{
    if (satir_adet >= satir_kap) {
        int yeni = satir_kap ? satir_kap * 2 : 128;
        size_t *g = realloc(satir_baslari, (size_t)yeni * sizeof(size_t));
        if (!g) return -1;
        satir_baslari = g;
        satir_kap = yeni;
    }
    satir_baslari[satir_adet++] = b;
    return 0;
}

static void satirlari_hesapla(int genislik)
{
    satir_adet = 0;
    if (!metin.veri || genislik < 1) return;
    size_t n = metin.uzunluk, bas = 0;
    int kol = 0;
    long son_bosluk = -1;

    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)metin.veri[i];
        if (c == '\n') {
            satir_ekle(bas); bas = i + 1; kol = 0; son_bosluk = -1;
            continue;
        }
        if ((c & 0xC0) == 0x80) continue;
        kol++;
        if (c == ' ') son_bosluk = (long)i;
        if (kol > genislik) {
            if (son_bosluk >= (long)bas) {
                satir_ekle(bas);
                bas = (size_t)son_bosluk + 1;
                kol = 0;
                for (size_t j = bas; j <= i; j++)
                    if ((((unsigned char)metin.veri[j]) & 0xC0) != 0x80) kol++;
            } else {
                satir_ekle(bas);
                bas = i;
                kol = 1;
            }
            son_bosluk = -1;
        }
    }
    if (bas < n) satir_ekle(bas);
}

/* ================= CIZIM ================= */

static void kisalt_yaz(const char *s, int sutun)
{
    int kol = 0;
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if ((c & 0xC0) != 0x80) { if (kol >= sutun) break; kol++; }
        putchar(c);
    }
}

static void icerik_satiri_yaz(int j)
{
    size_t b = satir_baslari[j];
    size_t e = (j + 1 < satir_adet) ? satir_baslari[j + 1] : metin.uzunluk;
    while (e > b && (metin.veri[e-1] == '\n' || metin.veri[e-1] == ' ')) e--;
    fwrite(metin.veri + b, 1, e - b, stdout);
}

static void arayuzu_ciz(void)
{
    int satir, sutun;
    boyut_al(&satir, &sutun);
    int yukseklik = satir - 4;

    if (hesap_genislik != sutun - 2) {
        satirlari_hesapla(sutun - 2);
        hesap_genislik = sutun - 2;
    }

    int toplam = (mod == 0) ? satir_adet : baglantilar.sayi;
    int *kay = (mod == 0) ? &kaydirma : &bag_kaydirma;
    int enb = toplam - yukseklik;
    if (enb < 0) enb = 0;
    if (*kay > enb) *kay = enb;
    if (*kay < 0)   *kay = 0;

    printf("\033[?25l" ZEMIN YAZI "\033[2J");

    printf("\033[1;1H" MAVI " ZEDIN BROWSER" GRI "  v%s", SURUM);
    if (durum_kodu) printf("  HTTP %d", durum_kodu);
    printf("\033[2;1H" YAZI " Adres: ");
    kisalt_yaz(simdiki_url[0] ? simdiki_url : "(bos)", sutun - 9);
    printf("\033[3;1H" GRI);
    for (int i = 0; i < sutun; i++) putchar('-');
    printf(YAZI);

    if (mod == 0) {
        if (!simdiki_url[0]) {
            printf("\033[6;3H" MAVI "Zedin'e hos geldin!" SIFIR YAZI);
            printf("\033[8;3HTurkce, sade ve kullanici odakli.");
        }
        for (int k = 0; k < yukseklik && *kay + k < satir_adet; k++) {
            printf("\033[%d;2H", 4 + k);
            icerik_satiri_yaz(*kay + k);
        }
    } else {
        for (int k = 0; k < yukseklik && *kay + k < baglantilar.sayi; k++) {
            printf("\033[%d;2H" GRI "%3d  " YAZI, 4 + k, *kay + k + 1);
            kisalt_yaz(baglantilar.adres[*kay + k], sutun - 8);
        }
        if (baglantilar.sayi == 0)
            printf("\033[5;3HBu sayfada baglanti yok.");
    }

    printf("\033[%d;1H" BAR "\033[2K ", satir);
    if (mesaj[0]) {
        kisalt_yaz(mesaj, sutun - 2);
    } else if (mod == 1) {
        printf("o:ac  j/k:kaydir  l:geri");
    } else if (sutun >= 60) {
        printf("g:adres l:linkler b:geri f:ileri r:yenile j/k:kaydir q:cikis  %d/%d",
               satir_adet ? *kay + 1 : 0, satir_adet);
    } else {
        printf("g l b f r j k q");
    }
    printf(SIFIR);
    fflush(stdout);
}

static int satir_oku(const char *ipucu, char *hedef, size_t boyut)
{
    size_t n = 0;
    int ciz = 1;
    hedef[0] = '\0';

    while (calisiyor) {
        if (ciz) {
            int satir, sutun;
            boyut_al(&satir, &sutun);
            printf("\033[%d;1H" BAR "\033[2K %s%s\033[?25h", satir, ipucu, hedef);
            fflush(stdout);
            ciz = 0;
        }
        int t = tus_oku();
        if (t == TUS_YOK) continue;
        if (t == TUS_KESME) { ciz = 1; continue; }
        if (t == TUS_HATA || t == 27) return 0;
        if (t == '\n' || t == '\r') return 1;
        if (t == 127 || t == 8) {
            while (n > 0 && ((unsigned char)hedef[n-1] & 0xC0) == 0x80) n--;
            if (n > 0) n--;
            hedef[n] = '\0';
            ciz = 1;
            continue;
        }
        if (t >= 32 && t < 256 && n + 1 < boyut) {
            hedef[n++] = (char)t;
            hedef[n] = '\0';
            ciz = 1;
        }
    }
    return 0;
}

/* ================= URL / GEZINME ================= */

static void url_coz(const char *taban, const char *goreli, char *cikti, size_t boyut)
{
    if (strncasecmp(goreli, "http://", 7) == 0 ||
        strncasecmp(goreli, "https://", 8) == 0) {
        snprintf(cikti, boyut, "%s", goreli);
        return;
    }
    const char *sema = strstr(taban, "://");
    if (!sema) { snprintf(cikti, boyut, "%s", goreli); return; }

    const char *host = sema + 3;
    const char *yol = strchr(host, '/');
    size_t kok = yol ? (size_t)(yol - taban) : strlen(taban);

    if (strncmp(goreli, "//", 2) == 0) {
        snprintf(cikti, boyut, "%.*s%s", (int)(sema + 1 - taban), taban, goreli);
    } else if (goreli[0] == '/') {
        snprintf(cikti, boyut, "%.*s%s", (int)kok, taban, goreli);
    } else {
        const char *son_slash = yol ? strrchr(yol, '/') : NULL;
        size_t dizin = son_slash ? (size_t)(son_slash - taban + 1) : kok;
        snprintf(cikti, boyut, "%.*s%s%s", (int)dizin, taban,
                 son_slash ? "" : "/", goreli);
    }
}

static int sayfa_getir(const char *url, int *durum)
{
    CHTTP_Response *response = NULL;
    CHTTP_String adres = { .ptr = (char *)url, .len = (int)strlen(url) };

    int sonuc = chttp_get(adres, NULL, 0, &response);
    if (sonuc != CHTTP_OK) return sonuc;

    *durum = response->status;
    tampon_temizle(&metin);
    baglantilari_temizle(&baglantilar);
    if (response->body.len > 0) {
        html_metne_cevir(response->body.ptr, response->body.len, &metin);
        baglantilari_topla(response->body.ptr, response->body.len, &baglantilar);
    }
    chttp_free_response(response);
    return CHTTP_OK;
}

static void sayfaya_git(const char *girdi, int gecmise_ekle)
{
    char url[URL_MAX];
    int durum = 0;

    snprintf(url, sizeof url, "%s", girdi);
    snprintf(mesaj, sizeof mesaj, "Yukleniyor: %s", url);
    arayuzu_ciz();

    int sonuc = sayfa_getir(url, &durum);
    if (sonuc != CHTTP_OK) {
        snprintf(mesaj, sizeof mesaj, "Hata: %s", chttp_strerror(sonuc));
        return;
    }

    snprintf(simdiki_url, sizeof simdiki_url, "%s", url);
    durum_kodu = durum;
    kaydirma = 0;
    hesap_genislik = 0;
    mod = 0;

    if (gecmise_ekle) {
        gecmis_konum++;
        if (gecmis_konum >= GECMIS_MAX) {
            memmove(gecmis[0], gecmis[1], (GECMIS_MAX - 1) * URL_MAX);
            gecmis_konum = GECMIS_MAX - 1;
        }
        snprintf(gecmis[gecmis_konum], URL_MAX, "%s", url);
        gecmis_sayi = gecmis_konum + 1;
    }
    snprintf(mesaj, sizeof mesaj, "HTTP %d - %d baglanti", durum, baglantilar.sayi);
}

static void baglantiyi_ac(int no)
{
    char url[URL_MAX];
    if (no < 1 || no > baglantilar.sayi) {
        snprintf(mesaj, sizeof mesaj, "Gecersiz numara");
        return;
    }
    url_coz(simdiki_url, baglantilar.adres[no - 1], url, sizeof url);
    sayfaya_git(url, 1);
}

static void adres_gir(void)
{
    char girdi[URL_MAX - 16], url[URL_MAX];
    if (!satir_oku("Adres: ", girdi, sizeof girdi) || !girdi[0]) return;
    if (strstr(girdi, "://")) snprintf(url, sizeof url, "%s", girdi);
    else                      snprintf(url, sizeof url, "https://%s", girdi);
    sayfaya_git(url, 1);
}

/* ================= ANA DONGU ================= */

int gui_calistir(void)
{
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        fprintf(stderr, "Zedin Browser bir terminalde calistirilmalidir.\n");
        return 1;
    }
    if (tcgetattr(STDIN_FILENO, &eski_terminal) == -1) {
        perror("Terminal ayarlari okunamadi");
        return 1;
    }

    struct termios yeni = eski_terminal;
    yeni.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    yeni.c_cc[VMIN] = 0;
    yeni.c_cc[VTIME] = 1;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &yeni) == -1) {
        perror("Terminal ayarlari degistirilemedi");
        return 1;
    }
    terminal_ayari = 1;

    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = boyut_degisti;
    sigaction(SIGWINCH, &sa, NULL);
    sa.sa_handler = cik;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);

    atexit(terminali_sifirla);
    printf("\033[?1049h");
    fflush(stdout);

    tampon_baslat(&metin);
    baglantilar.sayi = 0;

    while (calisiyor) {
        if (yeniden_ciz) { yeniden_ciz = 0; arayuzu_ciz(); }

        int t = tus_oku();
        if (t == TUS_YOK || t == TUS_KESME) continue;
        if (t == TUS_HATA) break;

        mesaj[0] = '\0';
        int k = (t < 256) ? tolower(t) : t;
        int satir, sutun;
        boyut_al(&satir, &sutun);
        int sayfa = satir - 5;
        if (sayfa < 1) sayfa = 1;
        int *kay = (mod == 0) ? &kaydirma : &bag_kaydirma;

        if (k == 'j' || k == TUS_ASAGI)             (*kay)++;
        else if (k == 'k' || k == TUS_YUKARI)       (*kay)--;
        else if (k == ' ' || k == TUS_SAYFA_ASAGI)  *kay += sayfa;
        else if (k == TUS_SAYFA_YUKARI)             *kay -= sayfa;
        else if (mod == 0) {
            if (k == 'q') {
                calisiyor = 0;
            } else if (k == 'g') {
                adres_gir();
            } else if (k == 'r' && simdiki_url[0]) {
                sayfaya_git(simdiki_url, 0);
            } else if (k == 'b' && gecmis_konum > 0) {
                gecmis_konum--;
                sayfaya_git(gecmis[gecmis_konum], 0);
            } else if (k == 'f' && gecmis_konum + 1 < gecmis_sayi) {
                gecmis_konum++;
                sayfaya_git(gecmis[gecmis_konum], 0);
            } else if (k == 'l') {
                mod = 1;
                bag_kaydirma = 0;
            }
        } else {
            if (k == 'l' || k == 'q' || k == 27) {
                mod = 0;
            } else if (k == 'o' || k == '\n' || k == '\r') {
                char s[16];
                if (satir_oku("Baglanti no: ", s, sizeof s) && s[0])
                    baglantiyi_ac(atoi(s));
            }
        }
        yeniden_ciz = 1;
    }

    tampon_temizle(&metin);
    baglantilari_temizle(&baglantilar);
    free(satir_baslari);
    return 0;
}
