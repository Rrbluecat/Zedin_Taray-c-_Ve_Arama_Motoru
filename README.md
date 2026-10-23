# Zedin Tarayıcı ve Arama Motoru

Zedin, C diliyle geliştirilen bir tarayıcı ve yerel arama motoru prototipidir. Projenin amacı, Türkçe odaklı, hafif ve kullanıcı kontrolünü önceleyen bir yazılım ekosistemi oluşturmaktır.

## Proje bileşenleri

### Zedin Browser
- C diliyle geliştirilir.
- cHTTP üzerinden HTTP istekleri gönderir.
- HTTPS desteğiyle derlenebilir.
- HTML etiketlerini ayıklayarak sayfa metnini göstermeyi hedefler.
- Terminal tabanlı bir arayüze sahiptir.

### Zedin Indexer
- Belirlenmiş kaynaklardan web sayfalarını toplar.
- Sayfa verilerini yerel `zedin_index.zidx` dosyasına kaydeder.
- İndeks dosyası çalışma sırasında oluşturulur; kaynak kodlarıyla birlikte yayımlanması gerekmez.

### Zedin Search
- Yerel indeks dosyasındaki içerik üzerinde arama yapar.
- İlgili sonuçları puanlayarak sıralar.

## Kaynak dosyaları

- `zedin_chttp.c`: Tarayıcının HTTP istemcisi ve metin işleme kodu.
- `zedin_gui.c`: Terminal arayüzü.
- `zedin_gui_main.c`: Arayüz giriş noktası.
- `html_entity.c` / `html_entity.h`: HTML karakter referanslarını işleme kodu.
- `zedin_indexer.c`: Yerel indeks oluşturucu.
- `zedin_search.c`: Yerel arama programı.
- `zedin_browser.c`: libcurl kullanan ayrı istemci uygulaması.
- `chttp.c` / `chttp.h`: Üçüncü taraf cHTTP kütüphanesi.

## Gereksinimler

Derleme için GCC veya uyumlu bir C derleyicisi gerekir. HTTPS kullanılan bileşenler ayrıca OpenSSL geliştirme dosyalarına ihtiyaç duyar. Eski istemci libcurl gerektirir.

## Derleme

### Tarayıcı

```sh
gcc -Wall -Wextra -std=c11 -DHTTPS_ENABLED \
    zedin_chttp.c zedin_gui.c html_entity.c chttp.c \
    -o zedin_gui -lssl -lcrypto
 İndeks olusturucuyu calistirmak icin;
gcc -Wall -Wextra -std=c11 -DHTTPS_ENABLED \
    zedin_indexer.c chttp.c \
    -o zedin_indexer -lssl -lcrypto
Zedin Search ü çalıştırmak (Arama motorunu çalıştırmak) için;
gcc -Wall -Wextra -std=c11 \
    zedin_search.c -o zedin_search
 Libcurl istemini çalıltırmak için;
gcc -Wall -Wextra -std=c11 \
    zedin_browser.c -o zedin_browser -lcurl
Derleme seçenekleri ve bağımlılıklar kullanılan işletim sistemine göre değişebilir.
Durum
Bu proje geliştirme aşamasındaki bir prototiptir. Tam kapsamlı, modern bir HTML/CSS/JavaScript tarayıcı motoru değildir. Uyumluluk, güvenlik, hata yönetimi ve performans geliştirmeleri devam etmektedir.
Arama motoru yerel bir indeks dosyası kullanır. İndeksin güncelliği ve kapsamı, indeks oluşturucunun çalıştırılmasına ve tanımlı kaynaklara bağlıdır.
Gizlilik yaklaşımı
Projenin hedeflerinden biri, kullanıcı verilerini mümkün olduğunca kullanıcının cihazında tutmak ve gereksiz veri toplamaktan kaçınmaktır. Bu hedef, mevcut prototipin bütün güvenlik ve gizlilik özelliklerinin tamamlandığı anlamına gelmez.
Üçüncü taraf bileşenler
cHTTP kaynak dosyalarının kendi lisans bildirimi chttp.c dosyasının sonunda yer alır. Bu bileşen Francesco Cozzuto tarafından yayımlanmıştır. Dağıtım sırasında ilgili lisans koşulları korunmalıdır. EOF
# Zedin_Taray-c-_Ve_Arama_Motoru
