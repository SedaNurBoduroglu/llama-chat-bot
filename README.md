# Llama Chatbot - Gemstone O1 (AM67A)

Llama chatbot, Nightrun'dan esinlenilerek oluşturulmuş, sadece CPU üzerinde çalışan bir chatbot uygulamasıdır. T3 Gemstone O1 kartları üzerinde Buildroot desteği ile Llama 3.2 1B Instruct modelini çalıştırır.

## İçindekiler

- [Performans, Donanım Kısıtlamaları ve Deneysel Notlar](#performans-donanım-kısıtlamaları-ve-deneysel-notlar)
- [Hazır İmaj](#hazır-i̇maj)
- [Depo İçeriği](#depo-repo-i̇çeriği)
- [Kendi İmajınızı Yaratmak İçin (Sıfırdan Kurulum)](#kendi-i̇majınızı-yaratmak-i̇çin-sıfırdan-kurulum)
- [Geliştirme (Kaynak Kodda Değişiklik Yapma)](#geliştirme-kaynak-kodda-değişiklik-yapma)
- [Uygulamayı Çalıştırma](#uygulamayı-çalıştırma)

## Performans, Donanım Kısıtlamaları ve Deneysel Notlar

Uygulamanın mimarisi ve cihazın donanım kısıtlamaları sebebiyle model yalnızca CPU üzerinde koşmaktadır. Yapılan testler ve donanımın yapısıyla ilgili gözlemler aşağıda özetlenmiştir.

### Token Üretim Hızı

Yapılan donanım testlerinde:

| Uygulama | Token Üretim Hızı |
|---|---|
| Nightrun | 0.4 - 0.8 token/sn |
| Llama Chatbot (bu proje) | **2.2 - 2.6 token/sn** |

### MMA ve TIDL Kısıtlamaları

AM67A üzerinde bulunan MMA (Matrix Multiply Accelerator) donanım hızlandırıcıları, mimari gereği yalnızca TI'ın kendi TIDL araçlarıyla çalışacak şekilde tasarlanmıştır. TIDL araçları temel olarak CNN gibi görece az kanallı görsel işleme modelleriyle çalışmak üzere optimize edilmiştir ve LLM (Büyük Dil Modelleri) gibi çok parametreli mimarileri desteklememektedir.

### GPU Desteği

LLM modellerini cihaz üzerinde optimize etmek için projede kullanılan `llama.cpp` kütüphanesi, cihaz üzerinde bulunan IMG BXS-4-64 model GPU'yu desteklememektedir.

## Hazır İmaj

Uygulamayı hızlıca denemek için hazır imajı bir SD karta flashlayın ve kartı T3 Gemstone O1 üzerinde başlatın.

- **İmaj Linki:** _Henüz eklenmedi._ İmaj dosyası (`sdcard.img`) boyutu nedeniyle bu depoda tutulmaz.

## Depo (Repo) İçeriği

Bu depo; uygulamanın kaynak kodunu (`src/main.cpp`), overlay hazırlama betiğini (`prepare_overlay.sh`) ve Buildroot'a entegre edilecek overlay (dosya sistemi katmanı) dizinini (`gemstone_overlay/`) içerir. Overlay dizininde uygulamanın çalışması için gereken paylaşımlı kütüphaneler (`llama.cpp`, `ggml`, `libgomp`, `libstdc++`) ve otomatik başlatma betiği (`root/.profile`) hazır olarak bulunur.

Depoda **bulunmayanlar** ve ayrıca edinilmesi gerekenler:

- **Buildroot** ve **`llama.cpp`** repoları: [Kurulum](#kendi-i̇majınızı-yaratmak-i̇çin-sıfırdan-kurulum) ve [Geliştirme](#geliştirme-kaynak-kodda-değişiklik-yapma) adımlarında indirilir.
- **LLM modeli:** Boyutu nedeniyle depoya eklenmemiştir. Llama 3.2 1B Instruct modelinin GGUF formatındaki `Q4_K_M` sürümünü indirip `gemstone_overlay/root/models/model-q4_k_m.gguf` olarak kaydedin. Uygulama açılışta modeli kart üzerinde `/root/models/model-q4_k_m.gguf` yolunda arar.
- **Derlenmiş uygulama (`gemstone_os_arm`):** [Geliştirme](#geliştirme-kaynak-kodda-değişiklik-yapma) adımlarıyla derlenip `prepare_overlay.sh` ile overlay'e (`gemstone_overlay/usr/bin/`) eklenir.

## Kendi İmajınızı Yaratmak İçin (Sıfırdan Kurulum)

1. Bu repoyu bilgisayarınıza indirin.
2. Gemstone destekleyen Buildroot deposunu, bu reponun yanına (aynı üst dizine) indirin: [t3gemstone/buildroot](https://github.com/t3gemstone/buildroot)

   ```bash
   git clone https://github.com/t3gemstone/buildroot.git
   ```
3. T3 Gemstone konfigürasyonunu Buildroot'a uygulamak için Buildroot ana dizininde şu komutu çalıştırın:

   ```bash
   make t3_gem_o1_defconfig
   ```

4. Repo içerisindeki overlay dizinini Buildroot'a tanıtmak için yapılandırma menüsünü açın:

   ```bash
   make menuconfig
   ```

   Menüden şu yolu izleyin: `System properties -> root filesystem overlay directories`

   Mevcut yolların yanına bir boşluk bırakarak `../llama-chat-bot/gemstone_overlay` yolunu ekleyin, kaydedin ve çıkın. (Bu yol Buildroot ana dizinine göre verilir; repoyu farklı bir yere indirdiyseniz yolu ona göre değiştirin.)

5. Derlemeden önce LLM modelinin ve derlenmiş uygulamanın overlay içinde olduğundan emin olun (bkz. [Depo İçeriği](#depo-repo-i̇çeriği)).

### Derleme ve U-Boot Yapılandırması

İmajı derlemek için doğrudan `make` komutunu çalıştırabilirsiniz. Eğer derleme işlemi hata verirse, U-Boot konfigürasyonunu güncellemek için Buildroot ana dizininde olduğunuzdan emin olun ve aşağıdaki komutları çalıştırıp tekrar derlemeyi deneyin:

```bash
# Buildroot dizininde olduğunuzdan emin olun ve U-Boot konfigürasyonunu güncellemek için şu komutları çalıştırın:
echo "BR2_TARGET_UBOOT_SPL=y" >> configs/t3_gem_o1_defconfig
echo 'BR2_TARGET_UBOOT_SPL_NAME="spl/u-boot-spl.bin tispl.bin"' >> configs/t3_gem_o1_defconfig
echo "BR2_TARGET_UBOOT_USE_BINMAN=y" >> configs/t3_gem_o1_defconfig
sed -i 's/BR2_TARGET_UBOOT_FORMAT_BIN=y/# BR2_TARGET_UBOOT_FORMAT_BIN is not set/' configs/t3_gem_o1_defconfig
echo "BR2_TARGET_UBOOT_FORMAT_IMG=y" >> configs/t3_gem_o1_defconfig
```

Derleme başarıyla tamamlandığında, `buildroot/output/images/` dizini altında `sdcard.img` isimli imaj dosyası oluşacaktır.

### SD Karta Yazdırma ve Başlatma

1. Oluşan boot edilebilir `sdcard.img` dosyasını `dd` komutu ile bir SD karta yazdırın:

   ```bash
   sudo dd if=output/images/sdcard.img of=/dev/sdX bs=1M && sync
   ```

   > **Not:** `/dev/sdX` kısmını SD kartınızın sisteminizdeki sürücü yoluna göre değiştirin.

2. SD kartı T3 Gemstone O1 kartına takın.
3. Terminal erişimi almak için UART kablosunu bağlayın.
4. Karta güç verin. Sistem Buildroot üzerinden başarılı bir şekilde boot edecektir.

## Geliştirme (Kaynak Kodda Değişiklik Yapma)

`llama.cpp` bu depoya dahil değildir; kaynak kodda değişiklik yapmadan önce ayrıca indirilmesi gerekir.

1. Proje dizininde, `external/` dizini altına `llama.cpp` reposunu indirin:

   ```bash
   git clone https://github.com/ggml-org/llama.cpp external/llama.cpp
   ```

2. `llama.cpp` kütüphanelerini kart için (aarch64) derleyin. Derleme sonunda `external/llama.cpp/build/bin/` altında `libllama.so`, `libggml*.so` dosyaları oluşmalıdır. Örnek bir çapraz derleme komutu:

   ```bash
   cd external/llama.cpp
   cmake -B build -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
         -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ \
         -DGGML_NATIVE=OFF -DLLAMA_CURL=OFF
   cmake --build build --config Release -j
   ```

3. Projenin kaynak kodlarında değişiklik yaptıktan sonra yeni çalıştırılabilir dosyayı üretmek için, `external/llama.cpp` dizini içerisindeyken aşağıdaki derleme komutunu çalıştırın:

   ```bash
   /usr/bin/aarch64-linux-gnu-g++ -O3 -std=c++17 ../../src/main.cpp -o ../../gemstone_os_arm -I include -I ggml/include -L build/bin -lllama -lggml -lggml-base -lggml-cpu -Wl,-rpath,build/bin
   ```

4. Yeni oluşan uygulamayı ve kütüphaneleri overlay içerisine eklemek için proje dizinindeki `prepare_overlay.sh` betiğini çalıştırın:

   ```bash
   cd ../..
   ./prepare_overlay.sh
   ```

> Modelin dosya yolunu veya adını değiştirmek isterseniz, bu ayarı `prepare_overlay.sh` betiğinin oluşturduğu `.profile` içindeki `MODEL_PATH` değişkeninden güncelleyebilirsiniz.

## Uygulamayı Çalıştırma

Kart başlatıldığında uygulama otomatik olarak açılmaya ayarlıdır. Uygulama herhangi bir sebepten açılmazsa veya mevcut uygulamayı kapatıp başka bir model ile çalıştırmak isterseniz, kart terminalinden şu komutu çalıştırabilirsiniz:

```bash
/usr/bin/gemstone_os_arm <model_path>
```

Sohbet sırasında `exit` yazarak veya `Ctrl+C` tuşlarına basarak uygulamadan çıkıp kartın komut satırına geçebilirsiniz; uygulamayı yukarıdaki komutla istediğiniz zaman (farklı bir modelle de) yeniden başlatabilirsiniz. Bağlam penceresi (1024 token) dolduğunda uygulama kapanır; yeniden başlatmanız gerekir.
