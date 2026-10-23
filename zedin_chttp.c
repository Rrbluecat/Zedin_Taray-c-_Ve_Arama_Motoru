#include <stdio.h>
#include <strings.h>
#include <string.h>
#include "chttp.h"
#include <stdlib.h>
#include "html_entity.h"
#define _POSIX_C_SOURCE 200809L
int gui_calistir(void);
void html_metnini_goster(char *html, int uzunluk)
{
   int i = 0;
   int bulundu = 0;
   int etiket_icinde;
   char karakter;
   int style_icinde = 0;
   int script_icinde = 0;
   int onceki_bosluk = 0;
   etiket_icinde = 0;
   for (int i = 0; i < uzunluk; i++){
       karakter = html[i];
       if (karakter == '<'){
          if (strncasecmp(html + i, "<script", 7) == 0){
             script_icinde = 1;
          }
          else if(strncasecmp(html + i, "</script", 8) == 0){
             script_icinde = 0;
          }
          if (strncasecmp(html + i, "<style", 6) == 0){
             style_icinde = 1;
          }
          else if(strncasecmp(html + i, "</style", 7) == 0){
             style_icinde = 0;
          }
          etiket_icinde = 1;
       }
       else if(karakter == '>'){
          etiket_icinde = 0;
       }
       else if (!etiket_icinde && !script_icinde && !style_icinde) {
          if (karakter == ' ' || karakter == '\t' || karakter == '\n' || karakter == '\r'){
          if(!onceki_bosluk){
             putchar(' ');
             onceki_bosluk = 1;
          }
    } else if (karakter == '&'){
      i = html_entity_yaz(html, i ,uzunluk, &onceki_bosluk);
   } else{
       putchar(karakter);
       onceki_bosluk = 0;
   }
  }
 }
}
void baglantilari_bul(char *adres, int adres_uzunlugu)
{
   char *aranan;
   char *konum;
   char *esittir;
   char *tirnak;
   char *tirnak2;
   int uzunluk;
   char *html = malloc((size_t)adres_uzunlugu + 1);
   if (html == NULL){
      fprintf(stderr, "Hata! Adres uzunlugu alinamadi: \n");
      return;
   }
   else{
   memcpy(html,adres,adres_uzunlugu);
   html[adres_uzunlugu] = '\0';
   aranan = html;
   while ((konum = strstr(aranan, "href")) != NULL){
   if (konum == NULL){
      fprintf(stderr, "Hata! Href konumu bulunamadi!: \n");
      free(html);
      return;
  }
   else {
      esittir = strchr(konum, '=');
      if (esittir == NULL){
         fprintf(stderr, "Hata! =  isareti bulunamadi!: \n");
         aranan = konum + 4;;
         return;
      }
      else{
         tirnak = strchr(esittir, '"');
         if (tirnak == NULL){
            fprintf(stderr, " Tirnak isareti bulunamadi!: \n");
         }
         else{
            tirnak++;
            tirnak2 = strchr(tirnak, '"');
            if (tirnak2 == NULL){
               fprintf(stderr,"İkinci tirnak bulunamadi!: \n"); 
            }
            else{
               fprintf(stderr, "İkinci tirnak bulundu: %c\n",*tirnak2);
               uzunluk = tirnak2 - tirnak;
               fprintf(stderr, "Uzunluk bulundu:%.*s\n",uzunluk,tirnak);
               fprintf(stderr, "Bulunan baglanti: %.*s\n", uzunluk, tirnak);
            }
            fprintf(stderr, " Tirnak isareti bulundu!: %c\n",tirnak[-1]);
         }
         fprintf(stderr, " = isareti bulundu: %c\n",*esittir);
      }
      fprintf(stderr, "Href konumu bulundu!: %.4s\n",konum);
      aranan = konum + 4;
   }
  }
 }
}
int main(void)
{
    return gui_calistir();
    size_t uzunluk = 0;
    size_t kapasite = 128;
    char *url_2 = malloc(kapasite);
    CHTTP_Response *response = NULL;
    int sonuc;
    int karakter;
    printf("Zedin Browser - URL giriniz: ");
    if (url_2 == NULL){
       fprintf(stderr, "Bos bir url olamaz!:\n");
       return 1;
    }
    while(1){
       karakter = getchar();
       if (karakter == '\n' || karakter == EOF){
          break;
       }
       if (uzunluk + 1 >= kapasite){
          size_t yeni_kapasite = kapasite * 2;
          char *gecici = realloc(url_2, yeni_kapasite);
          if (gecici == NULL){
          free(url_2);
          return 1;
          }
          url_2 = gecici;
          kapasite = yeni_kapasite;
       }
       url_2[uzunluk] = (char)karakter;
       uzunluk++;
    }
    if (url_2 == NULL){
       fprintf(stderr, "Hata: Url bos!");
       return 1;
    }
    url_2[uzunluk] = '\0';
    if (url_2[0] == '\0'){
       fprintf(stderr, "Hata url satırı bos olamaz!: ");
       free(url_2);
       return 1;
    }
    CHTTP_String adres = {
        .ptr = url_2,
        .len = (int)strlen(url_2)
    };

    sonuc = chttp_get(adres, NULL, 0, &response);

    if (sonuc != CHTTP_OK) {
        fprintf(stderr, "Istek basarisiz: %s\n",
                chttp_strerror(sonuc));
        return 1;
    }

    printf("\nHTTP Durum Kodu: %d\n", response->status);
    printf("Durum: %.*s\n\n",
           response->reason.len,
           response->reason.ptr);

    if (response->body.len > 0) {
    html_metnini_goster(response->body.ptr, response->body.len);
        putchar('\n');
    }

    baglantilari_bul(response->body.ptr,response->body.len);

    chttp_free_response(response);
    return 0;
}
