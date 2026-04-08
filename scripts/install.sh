#!/bin/bash
# LinuxComplete — Installation Script / Kurulum Scripti
# System-wide text prediction for Linux (Wayland)

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}"
echo "╔══════════════════════════════════════════╗"
echo "║         LinuxComplete Installer          ║"
echo "║   System-wide Text Prediction Engine     ║"
echo "║   Sistem Genelinde Metin Tamamlama       ║"
echo "╚══════════════════════════════════════════╝"
echo -e "${NC}"

# Check if running as root
if [ "$EUID" -eq 0 ]; then
    echo -e "${RED}Do not run as root. / Root olarak çalıştırmayın.${NC}"
    exit 1
fi

# Detect distro
DISTRO="unknown"
if command -v pacman &>/dev/null; then
    DISTRO="arch"
elif command -v apt &>/dev/null; then
    DISTRO="debian"
elif command -v dnf &>/dev/null; then
    DISTRO="fedora"
fi

echo -e "${BLUE}[1/5]${NC} Checking dependencies / Bağımlılıklar kontrol ediliyor..."

install_deps() {
    case "$DISTRO" in
        arch)
            local missing=()
            for pkg in fcitx5 cmake gcc nlohmann-json; do
                if ! pacman -Qi "$pkg" &>/dev/null; then
                    missing+=("$pkg")
                fi
            done
            if [ ${#missing[@]} -gt 0 ]; then
                echo -e "${YELLOW}Installing: ${missing[*]}${NC}"
                sudo pacman -S --noconfirm "${missing[@]}"
            fi
            ;;
        debian)
            sudo apt-get install -y fcitx5 fcitx5-dev cmake g++ nlohmann-json3-dev
            ;;
        fedora)
            sudo dnf install -y fcitx5 fcitx5-devel cmake gcc-c++ json-devel
            ;;
        *)
            echo -e "${RED}Unsupported distro. Install manually: fcitx5, cmake, gcc, nlohmann-json${NC}"
            echo -e "${RED}Desteklenmeyen dağıtım. Manuel kurun: fcitx5, cmake, gcc, nlohmann-json${NC}"
            exit 1
            ;;
    esac
}

install_deps
echo -e "${GREEN}✓ Dependencies OK / Bağımlılıklar tamam${NC}"

echo -e "${BLUE}[2/5]${NC} Building / Derleniyor..."

BUILD_DIR="$(pwd)/build"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake .. -DCMAKE_INSTALL_PREFIX=/usr
make -j"$(nproc)"

echo -e "${GREEN}✓ Build OK / Derleme tamam${NC}"

echo -e "${BLUE}[3/5]${NC} Installing / Kuruluyor..."
sudo make install

echo -e "${GREEN}✓ Install OK / Kurulum tamam${NC}"

echo -e "${BLUE}[4/5]${NC} Setting up environment / Ortam ayarlanıyor..."

# Add IM environment variables
SHELL_RC=""
if [ -f "$HOME/.zshrc" ]; then
    SHELL_RC="$HOME/.zshrc"
elif [ -f "$HOME/.bashrc" ]; then
    SHELL_RC="$HOME/.bashrc"
fi

if [ -n "$SHELL_RC" ]; then
    if ! grep -q "GTK_IM_MODULE=fcitx" "$SHELL_RC" 2>/dev/null; then
        echo "" >> "$SHELL_RC"
        echo "# LinuxComplete — Input Method Configuration" >> "$SHELL_RC"
        echo "export GTK_IM_MODULE=fcitx" >> "$SHELL_RC"
        echo "export QT_IM_MODULE=fcitx" >> "$SHELL_RC"
        echo "export XMODIFIERS=@im=fcitx" >> "$SHELL_RC"
        echo "export SDL_IM_MODULE=fcitx" >> "$SHELL_RC"
        echo "export GLFW_IM_MODULE=ibus" >> "$SHELL_RC"
        echo -e "${GREEN}✓ Environment variables added to $SHELL_RC${NC}"
    else
        echo -e "${YELLOW}Environment variables already set / Ortam değişkenleri zaten ayarlı${NC}"
    fi
fi

echo -e "${BLUE}[5/5]${NC} Creating user data directory / Kullanıcı veri dizini oluşturuluyor..."
mkdir -p "$HOME/.local/share/linuxcomplete/user"

echo ""
echo -e "${GREEN}╔══════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║     Installation Complete! / Tamam!      ║${NC}"
echo -e "${GREEN}╚══════════════════════════════════════════╝${NC}"
echo ""
echo -e "Next steps / Sonraki adımlar:"
echo -e "  1. ${YELLOW}source $SHELL_RC${NC}  (or restart shell / veya shell'i yeniden başlat)"
echo -e "  2. ${YELLOW}fcitx5 -d${NC}  (start Fcitx5 / Fcitx5'i başlat)"
echo -e "  3. ${YELLOW}fcitx5-configtool${NC}  (add LinuxComplete as input method)"
echo -e "     (LinuxComplete'i giriş yöntemi olarak ekle)"
echo ""
echo -e "  Toggle: ${YELLOW}Ctrl+Space${NC}  |  Accept: ${YELLOW}Tab${NC}  |  Dismiss: ${YELLOW}Esc${NC}"
