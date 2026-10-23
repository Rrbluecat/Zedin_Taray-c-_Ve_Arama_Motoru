#ifndef HTML_ENTITY_H
#define HTML_ENTITY_H

/* html[i] == '&' olmalı. Çözümü stdout'a yazar.
   Dönüş: tüketilen son karakterin indeksi (genelde ';').
   Varlık değilse '&' yazar ve i'yi döndürür. */
int html_entity_yaz(const char *html, int i, int uzunluk, int *onceki_bosluk);

#endif
