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
