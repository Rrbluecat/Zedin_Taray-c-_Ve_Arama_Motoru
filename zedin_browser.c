#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <curl/curl.h>
struct MemoryStruct{
  char *memory;
  size_t size;
};
int main(void)
{
    char url[2048];
    CURL *curl;
    CURLcode sonuc;

    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        fprintf(stderr, "libcurl başlatılamadı.\n");
        return 1;
    }

    curl = curl_easy_init();

    if (curl == NULL) {
        fprintf(stderr, "İstek nesnesi oluşturulamadı.\n");
        return 1;
    }

    fprintf(stderr, "Lutfen gitmek istediginiz url adresini yaziniz: ");
    if (fgets(url, sizeof(url), stdin) == NULL){
    	fprintf(stderr, "Fgets islemi urlyi alamadi!: \n");
    	return 1;
    }
    url[strcspn(url, "\n")] = '\0';
    if (url[0] == '\0'){
       fprintf(stderr, "Dostum bu bos bir url! Dolu bir url ver");
       return 1;
    }
    else {
       curl_easy_setopt(curl, CURLOPT_URL, url);
       sonuc = curl_easy_perform(curl);
       if (sonuc != CURLE_OK){
          fprintf(stderr, "Curl calisma hatasi: %s\n",
                   curl_easy_strerror(sonuc));
       }
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    }
    return 0;
}
