#!/bin/bash

# Hata durumunda scripti durdur
set -e

echo -e "\n🚀 [GEMSTONE OS] Buildroot Overlay Hazırlama Scripti Başlıyor...\n"

# Yollar (Çalışma ortamına göre)
WORKSPACE_DIR="/home/seda/workspace/t3_internship/llama_chat_workspace"
OVERLAY_DIR="$WORKSPACE_DIR/gemstone_overlay"

# Kaynak dosyaların yolları
EXE_FILE="$WORKSPACE_DIR/gemstone_os_arm"
LIB_LLAMA="$WORKSPACE_DIR/external/llama.cpp/build/bin/libllama.so"
LIB_GGML="$WORKSPACE_DIR/external/llama.cpp/build/bin/libggml.so"

# 1. Overlay Klasör Ağacını Oluştur
echo "📂 Klasör ağacı oluşturuluyor..."
mkdir -p "$OVERLAY_DIR/usr/bin"
mkdir -p "$OVERLAY_DIR/usr/lib"
mkdir -p "$OVERLAY_DIR/root"

# 2. Dosyaları Kopyala
echo "⚙️ Çalıştırılabilir dosya kopyalanıyor..."
if [ -f "$EXE_FILE" ]; then
    cp "$EXE_FILE" "$OVERLAY_DIR/usr/bin/"
    # İzinleri ayarla (Linux için kritik)
    chmod +x "$OVERLAY_DIR/usr/bin/gemstone_os_arm"
else
    echo "❌ HATA: gemstone_os_arm bulunamadı! Derleme işlemini kontrol et."
    exit 1
fi

echo "📚 Kütüphaneler kopyalanıyor..."
if [ -f "$LIB_LLAMA" ] && [ -f "$LIB_GGML" ]; then
    cp "$LIB_LLAMA" "$OVERLAY_DIR/usr/lib/"
    cp "$LIB_GGML" "$OVERLAY_DIR/usr/lib/"
else
    echo "❌ HATA: .so kütüphaneleri bulunamadı! llama.cpp CMake derlemesini kontrol et."
    exit 1
fi

# 3. .profile Dosyasını Oluştur (Otomatik Başlatma)
echo "📜 .profile (otomatik başlatma) betiği oluşturuluyor..."
cat << 'EOF' > "$OVERLAY_DIR/root/.profile"
#!/bin/sh

# Renk kodları (Terminal dostu)
C_CYAN="\e[1;36m"
C_RED="\e[1;31m"
C_RESET="\e[0m"

#echo -e "${C_CYAN}[GEMSTONE OS] Sistem başlatıldı, yapay zeka co-pilot aranıyor...${C_RESET}"

# Modelin SD karttaki yolu 
MODEL_PATH="/root/models/model-q4_k_m.gguf"

if [ -f "$MODEL_PATH" ]; then
    # Model bulunursa uygulamayı başlat
    /usr/bin/gemstone_os_arm "$MODEL_PATH"
else
    # Model bulunamazsa Linux kilitlenmesin, komut satırına düşsün diye hata basıp bırakıyoruz
    echo -e "${C_RED}[FATAL ERROR] Model bulunamadı! SD kartı ve $MODEL_PATH yolunu kontrol edin.${C_RESET}"
fi
EOF

echo -e "\n✅ BAŞARILI! Overlay klasörü şurada hazırlandı: $OVERLAY_DIR"
echo "👉 Tek yapman gereken bu klasörün içindeki (usr ve root) dosyaları Buildroot'un ana overlay klasörüne kopyalamak."
