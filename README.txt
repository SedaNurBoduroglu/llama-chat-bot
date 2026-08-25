====================================================
 LLAMA CHATBOT - GEMSTONE O1 (AM67A)
====================================================

Llama chatbot, Nightrun'dan esinlenilerek oluşturulmuş, sadece CPU
üzerinde çalışan bir chatbot uygulamasıdır. T3 Gemstone O1 kartları
üzerinde Buildroot desteği ile Llama 3.2 1B Instruct modelini çalıştırır.


İÇİNDEKİLER
----------------------------------------------------
1. Performans, Donanım Kısıtlamaları ve Deneysel Notlar
2. Hazır İmaj
3. Depo (Repo) İçeriği
4. Kendi İmajınızı Yaratmak İçin (Sıfırdan Kurulum)
5. Geliştirme (Kaynak Kodda Değişiklik Yapma)
6. Uygulamayı Çalıştırma


1. PERFORMANS, DONANIM KISITLAMALARI VE DENEYSEL NOTLAR
----------------------------------------------------
Uygulamanın mimarisi ve cihazın donanım kısıtlamaları sebebiyle model
yalnızca CPU üzerinde koşmaktadır. Yapılan testler ve donanımın
yapısıyla ilgili gözlemler aşağıda özetlenmiştir.

  Token Üretim Hızı
  ~~~~~~~~~~~~~~~~~
  Yapılan donanım testlerinde:

    - Nightrun                    : 0.4 - 0.8 token/sn
    - Llama Chatbot (bu proje)    : 2.2 - 2.6 token/sn

  MMA ve TIDL Kısıtlamaları
  ~~~~~~~~~~~~~~~~~~~~~~~~~
  AM67A üzerinde bulunan MMA (Matrix Multiply Accelerator) donanım
  hızlandırıcıları, mimari gereği yalnızca TI'ın kendi TIDL
  araçlarıyla çalışacak şekilde tasarlanmıştır. TIDL araçları temel
  olarak CNN gibi görece az kanallı görsel işleme modelleriyle
  çalışmak üzere optimize edilmiştir ve LLM (Büyük Dil Modelleri)
  gibi çok parametreli mimarileri desteklememektedir.

  GPU Desteği
  ~~~~~~~~~~~
  LLM modellerini cihaz üzerinde optimize etmek için projede
  kullanılan llama.cpp kütüphanesi, cihaz üzerinde bulunan
  IMG BXS-4-64 model GPU'yu desteklememektedir.


2. HAZIR İMAJ
----------------------------------------------------
Uygulamayı hızlıca denemek için hazır imajı bir SD karta flashlayın
ve kartı T3 Gemstone O1 üzerinde başlatın.

  İmaj Linki: [Google Drive Linki Buraya Eklenecek]


3. DEPO (REPO) İÇERİĞİ
----------------------------------------------------
Bu depo; proje içinde kullanılan kaynak kodları, LLM modelini ve
ihtiyaç duyulan dış kaynakları barındırır. İçerisinde Buildroot
reposu ve llama.cpp reposu BULUNMAZ, ancak Buildroot'a entegre
edilecek overlay (dosya sistemi katmanı) dizinini içerir. llama.cpp,
"Geliştirme" bölümündeki adımlarda ayrıca indirilmelidir.


4. KENDİ İMAJINIZI YARATMAK İÇİN (SIFIRDAN KURULUM)
----------------------------------------------------
  1) Bu repoyu bilgisayarınıza indirin.

  2) Gemstone destekleyen Buildroot deposunu indirin:
     [Buildroot Linki Buraya Eklenecek]

  3) T3 Gemstone konfigürasyonunu Buildroot'a uygulamak için
     Buildroot ana dizininde şu komutu çalıştırın:

         make t3_gem_o1_defconfig

  4) Repo içerisindeki overlay dizinini Buildroot'a tanıtmak için
     yapılandırma menüsünü açın:

         make menuconfig

     Menüden şu yolu izleyin:
         System properties -> root filesystem overlay directories

     Mevcut yolların yanına bir boşluk bırakarak
     "../llama_chat_workspace/gemstone_overlay" yolunu ekleyin,
     kaydedin ve çıkın.

  Derleme ve U-Boot Yapılandırması
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  İmajı derlemek için doğrudan "make" komutunu çalıştırabilirsiniz.
  Eğer derleme işlemi hata verirse, U-Boot konfigürasyonunu
  güncellemek için Buildroot ana dizininde olduğunuzdan emin olun ve
  aşağıdaki komutları çalıştırıp tekrar derlemeyi deneyin:

      # Buildroot dizininde olduğunuzdan emin olun ve U-Boot
      # konfigürasyonunu güncellemek için şu komutları çalıştırın:
      echo "BR2_TARGET_UBOOT_SPL=y" >> configs/t3_gem_o1_defconfig
      echo 'BR2_TARGET_UBOOT_SPL_NAME="spl/u-boot-spl.bin tispl.bin"' >> configs/t3_gem_o1_defconfig
      echo "BR2_TARGET_UBOOT_USE_BINMAN=y" >> configs/t3_gem_o1_defconfig
      sed -i 's/BR2_TARGET_UBOOT_FORMAT_BIN=y/# BR2_TARGET_UBOOT_FORMAT_BIN is not set/' configs/t3_gem_o1_defconfig
      echo "BR2_TARGET_UBOOT_FORMAT_IMG=y" >> configs/t3_gem_o1_defconfig

  Derleme başarıyla tamamlandığında, buildroot/output/images/
  dizini altında "sdcard.img" isimli imaj dosyası oluşacaktır.

  SD Karta Yazdırma ve Başlatma
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  1) Oluşan boot edilebilir "sdcard.img" dosyasını dd komutu ile bir
     SD karta yazdırın:

         sudo dd if=output/images/sdcard.img of=/dev/sdX bs=1M && sync

     Not: "/dev/sdX" kısmını SD kartınızın sisteminizdeki sürücü
     yoluna göre değiştirin.

  2) SD kartı T3 Gemstone O1 kartına takın.

  3) Terminal erişimi almak için UART kablosunu bağlayın.

  4) Karta güç verin. Sistem Buildroot üzerinden başarılı bir
     şekilde boot edecektir.


5. GELİŞTİRME (KAYNAK KODDA DEĞİŞİKLİK YAPMA)
----------------------------------------------------
llama.cpp bu depoya dahil değildir; kaynak kodda değişiklik
yapmadan önce ayrıca indirilmesi gerekir.

  1) "external/" dizini altına llama.cpp reposunu indirin:

         git clone https://github.com/ggml-org/llama.cpp external/llama.cpp

  2) Projenin kaynak kodlarında değişiklik yaptıktan sonra yeni
     çalıştırılabilir dosyayı üretmek için, llama.cpp dizini
     içerisindeyken aşağıdaki derleme komutunu çalıştırın:

         cd external/llama.cpp
         /usr/bin/aarch64-linux-gnu-g++ -O3 -std=c++17 ../../src/main.cpp -o ../../gemstone_os_arm -I include -I ggml/include -L build/bin -lllama -lggml -lggml-base -lggml-cpu -Wl,-rpath,build/bin

  3) Yeni oluşan uygulamayı overlay içerisine eklemek için proje
     dizinindeki "prepare_overlay" betiğini çalıştırın.

  Not: Modelin dosya yolunu veya adını değiştirmek isterseniz, bu
  ayarı doğrudan "prepare_overlay" betiğinin içinden
  güncelleyebilirsiniz.


6. UYGULAMAYI ÇALIŞTIRMA
----------------------------------------------------
Kart başlatıldığında uygulama otomatik olarak açılmaya ayarlıdır.
Uygulama herhangi bir sebepten açılmazsa veya mevcut uygulamayı
kapatıp başka bir model ile çalıştırmak isterseniz, kart
terminalinden şu komutu çalıştırabilirsiniz:

    /usr/bin/gemstone_os_arm <model_path>
