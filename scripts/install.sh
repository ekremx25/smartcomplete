#!/usr/bin/env bash
# SmartComplete — One-Click Installer
# System-wide text prediction for Linux (Wayland + X11).

set -Eeuo pipefail

# ── Colors / UI helpers ────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
LOG_FILE="$PROJECT_DIR/install.log"

print_banner() {
    echo -e "${CYAN}${BOLD}"
    echo "  ┌─────────────────────────────────────────┐"
    echo "  │          SmartComplete Installer         │"
    echo "  │    System-wide Text Prediction Engine    │"
    echo "  └─────────────────────────────────────────┘"
    echo -e "${NC}"
}

TOTAL_STEPS=7
step() { echo -e "\n${BLUE}[$1/${TOTAL_STEPS}]${NC} ${BOLD}$2${NC}"; }
ok()   { echo -e "    ${GREEN}✓${NC} $1"; }
warn() { echo -e "    ${YELLOW}!${NC} $1"; }
fail() { echo -e "    ${RED}✗${NC} $1"; echo -e "    See ${BOLD}$LOG_FILE${NC} for details."; exit 1; }

# ── Error trap: dump log tail on failure ──────────────────────
on_error() {
    local exit_code=$?
    local line_no=$1
    echo ""
    echo -e "${RED}${BOLD}Installation failed${NC} (exit $exit_code, line $line_no)"
    if [ -f "$LOG_FILE" ]; then
        echo -e "${YELLOW}Last 20 lines of $LOG_FILE:${NC}"
        tail -20 "$LOG_FILE" | sed 's/^/    /'
    fi
    exit $exit_code
}
trap 'on_error $LINENO' ERR

# Redirect detailed logs (build output, etc.) to the log file.
: > "$LOG_FILE"

# ── Preflight ──────────────────────────────────────────────────
print_banner

if [ "${EUID:-$(id -u)}" -eq 0 ]; then
    echo -e "${RED}Do not run as root. The script will invoke sudo when needed.${NC}"
    exit 1
fi

if [ ! -f "$PROJECT_DIR/CMakeLists.txt" ]; then
    echo -e "${RED}Cannot find CMakeLists.txt. Run this from the project directory.${NC}"
    exit 1
fi

# Detect distro.
DISTRO="unknown"
if command -v pacman &>/dev/null; then DISTRO="arch"
elif command -v apt-get &>/dev/null; then DISTRO="debian"
elif command -v dnf &>/dev/null; then DISTRO="fedora"
fi

# Detect compositor / session type.
COMPOSITOR="unknown"
SESSION_TYPE="${XDG_SESSION_TYPE:-unknown}"
if pgrep -x Hyprland &>/dev/null; then COMPOSITOR="hyprland"
elif pgrep -x niri &>/dev/null; then COMPOSITOR="niri"
elif pgrep -x sway &>/dev/null; then COMPOSITOR="sway"
elif pgrep -x gnome-shell &>/dev/null; then COMPOSITOR="gnome"
elif pgrep -x plasmashell &>/dev/null; then COMPOSITOR="kde"
elif [ "$SESSION_TYPE" = "x11" ]; then COMPOSITOR="x11"
fi

# Detect shell RC.
SHELL_RC=""
case "${SHELL:-}" in
    */zsh)  [ -f "$HOME/.zshrc" ] && SHELL_RC="$HOME/.zshrc" ;;
    */bash) [ -f "$HOME/.bashrc" ] && SHELL_RC="$HOME/.bashrc" ;;
    *)
        [ -f "$HOME/.zshrc" ] && SHELL_RC="$HOME/.zshrc"
        [ -z "$SHELL_RC" ] && [ -f "$HOME/.bashrc" ] && SHELL_RC="$HOME/.bashrc"
        ;;
esac

echo -e "  Distro:       ${BOLD}$DISTRO${NC}"
echo -e "  Compositor:   ${BOLD}$COMPOSITOR${NC}"
echo -e "  Session type: ${BOLD}$SESSION_TYPE${NC}"
echo -e "  Shell RC:     ${BOLD}${SHELL_RC:-none detected}${NC}"
echo -e "  Project:      ${BOLD}$PROJECT_DIR${NC}"
echo -e "  Log file:     ${BOLD}$LOG_FILE${NC}"

# ── Step 1: Install dependencies ──────────────────────────────
step 1 "Installing dependencies..."

case "$DISTRO" in
    arch)
        missing=()
        for pkg in fcitx5 cmake gcc nlohmann-json; do
            pacman -Qi "$pkg" &>/dev/null || missing+=("$pkg")
        done
        if [ ${#missing[@]} -gt 0 ]; then
            echo -e "    Installing: ${YELLOW}${missing[*]}${NC}"
            sudo pacman -S --noconfirm "${missing[@]}" >>"$LOG_FILE" 2>&1
        fi
        ok "Dependencies installed"
        ;;
    debian)
        sudo apt-get update -qq >>"$LOG_FILE" 2>&1
        sudo apt-get install -y -qq \
            fcitx5 libfcitx5core-dev fcitx5-modules-dev \
            fcitx5-frontend-qt5 fcitx5-frontend-gtk3 fcitx5-config-qt \
            cmake g++ nlohmann-json3-dev >>"$LOG_FILE" 2>&1
        ok "Dependencies installed"
        ;;
    fedora)
        sudo dnf install -y -q \
            fcitx5 fcitx5-devel fcitx5-qt fcitx5-gtk \
            fcitx5-configtool cmake gcc-c++ json-devel >>"$LOG_FILE" 2>&1
        ok "Dependencies installed"
        ;;
    *)
        fail "Unsupported distro. Install manually: fcitx5 (+dev headers), cmake, gcc, nlohmann-json"
        ;;
esac

# ── Step 2: Build ─────────────────────────────────────────────
step 2 "Building from source..."

BUILD_DIR="$PROJECT_DIR/build"
mkdir -p "$BUILD_DIR"

# Configure.
if ! cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_BUILD_TYPE=Release >>"$LOG_FILE" 2>&1; then
    fail "CMake configuration failed"
fi
ok "Configured"

# Build.
if ! cmake --build "$BUILD_DIR" -j"$(nproc)" >>"$LOG_FILE" 2>&1; then
    fail "Build failed"
fi
ok "Built ($(find "$BUILD_DIR" -name 'linuxcomplete.so' | head -1 | xargs -I{} du -h {} 2>/dev/null | cut -f1))"

# Smoke-test: run the test suite.
if ! (cd "$BUILD_DIR" && ctest --output-on-failure) >>"$LOG_FILE" 2>&1; then
    warn "Test suite reported failures (continuing anyway)"
else
    ok "Test suite passed"
fi

# ── Step 3: Install ───────────────────────────────────────────
step 3 "Installing to system..."

if ! sudo cmake --install "$BUILD_DIR" >>"$LOG_FILE" 2>&1; then
    fail "System install failed"
fi

# Create user data directories.
mkdir -p "$HOME/.local/share/linuxcomplete/user"
mkdir -p "$HOME/.config/linuxcomplete"

ok "Installed to /usr"
ok "User data directory ready"

# ── Step 4: Configure environment variables ───────────────────
step 4 "Configuring environment variables..."

if [ -n "$SHELL_RC" ]; then
    if grep -q "GTK_IM_MODULE=fcitx" "$SHELL_RC" 2>/dev/null; then
        ok "Environment variables already configured in $SHELL_RC"
    else
        cat >> "$SHELL_RC" << 'ENVBLOCK'

# SmartComplete — Input Method Configuration
export GTK_IM_MODULE=fcitx
export QT_IM_MODULE=fcitx
export XMODIFIERS=@im=fcitx
export SDL_IM_MODULE=fcitx
export GLFW_IM_MODULE=ibus
ENVBLOCK
        ok "Environment variables added to $SHELL_RC"
    fi
    export GTK_IM_MODULE=fcitx QT_IM_MODULE=fcitx XMODIFIERS=@im=fcitx
    export SDL_IM_MODULE=fcitx GLFW_IM_MODULE=ibus
else
    warn "No shell RC file detected. Add these to your shell profile:"
    cat <<'EOF'
      export GTK_IM_MODULE=fcitx
      export QT_IM_MODULE=fcitx
      export XMODIFIERS=@im=fcitx
      export SDL_IM_MODULE=fcitx
      export GLFW_IM_MODULE=ibus
EOF
fi

# ── Step 5: Configure Fcitx5 profile ──────────────────────────
step 5 "Configuring Fcitx5 profile..."

killall fcitx5 2>/dev/null || true
sleep 0.5

mkdir -p "$HOME/.config/fcitx5"
PROFILE="$HOME/.config/fcitx5/profile"

if [ -f "$PROFILE" ] && grep -q "DefaultIM=linuxcomplete" "$PROFILE"; then
    ok "Fcitx5 profile already set to SmartComplete"
else
    # Preserve existing profile as a backup.
    [ -f "$PROFILE" ] && cp "$PROFILE" "$PROFILE.bak.$(date +%s)"
    cat > "$PROFILE" << 'EOF'
[Groups/0]
Name=Default
Default Layout=us
DefaultIM=linuxcomplete

[Groups/0/Items/0]
Name=keyboard-us
Layout=us

[Groups/0/Items/1]
Name=linuxcomplete
Layout=

[GroupOrder]
0=Default
EOF
    ok "Fcitx5 profile configured (SmartComplete as default IM)"
fi

# Global config: make IM active by default across all windows.
CONFIG="$HOME/.config/fcitx5/config"
if [ -f "$CONFIG" ] && grep -q "ShareInputState=All" "$CONFIG"; then
    ok "Fcitx5 global config already has shared input state"
else
    [ -f "$CONFIG" ] && cp "$CONFIG" "$CONFIG.bak.$(date +%s)"
    cat > "$CONFIG" << 'EOF'
[Hotkey]
EnumerateWithTriggerKeys=True
AltTriggerKeys=Shift_L

[Behavior]
# Share active/inactive state across all windows — once activated, stays on.
ShareInputState=All
PreloadInputMethod=True
ShowFirstInputMethodInformation=True
DefaultInputMethod=linuxcomplete
EOF
    ok "Fcitx5 global config written (shared state — IM stays active everywhere)"
fi

# Compositor autostart.
add_autostart_line() {
    local file="$1" marker="$2" line="$3"
    if [ ! -f "$file" ]; then
        warn "$file not found — add manually: $line"
        return
    fi
    if grep -q "fcitx5" "$file"; then
        ok "Autostart already present in $file"
    else
        {
            echo ""
            echo "$marker"
            echo "$line"
        } >> "$file"
        ok "Autostart added to $file"
    fi
}

add_hyprland_env_block() {
    local f="$1"
    if grep -q "^env = GTK_IM_MODULE" "$f" 2>/dev/null; then
        ok "Hyprland IM env already set in $f"
        return
    fi
    cat >> "$f" << 'EOF'

# SmartComplete — Input Method environment (session-wide, includes GUI apps)
env = GTK_IM_MODULE,fcitx
env = QT_IM_MODULE,fcitx
env = XMODIFIERS,@im=fcitx
env = SDL_IM_MODULE,fcitx
env = GLFW_IM_MODULE,ibus
EOF
    ok "Hyprland IM env block added to $f"
}

case "$COMPOSITOR" in
    hyprland)
        for f in "$HOME/.config/hypr/execs.conf" "$HOME/.config/hypr/hyprland.conf"; do
            if [ -f "$f" ]; then
                add_autostart_line "$f" "# SmartComplete" "exec-once = fcitx5 -d"
                # Also auto-activate IM after fcitx5 starts — otherwise
                # apps start in "keyboard-only" mode until user presses Ctrl+Space.
                if ! grep -q "fcitx5-remote -o" "$f"; then
                    echo "exec-once = sleep 2 && fcitx5-remote -o" >> "$f"
                    ok "Auto-activation added to $f"
                fi
                add_hyprland_env_block "$f"
                break
            fi
        done
        ;;
    niri)
        add_autostart_line "$HOME/.config/niri/config.kdl" \
            "// SmartComplete" 'spawn-at-startup "fcitx5" "-d"'
        ;;
    sway)
        add_autostart_line "$HOME/.config/sway/config" \
            "# SmartComplete" "exec fcitx5 -d"
        ;;
    gnome|kde)
        # Create a standard XDG autostart entry.
        mkdir -p "$HOME/.config/autostart"
        DESKTOP_FILE="$HOME/.config/autostart/fcitx5.desktop"
        if [ ! -f "$DESKTOP_FILE" ]; then
            cat > "$DESKTOP_FILE" << 'EOF'
[Desktop Entry]
Type=Application
Name=Fcitx5 (SmartComplete)
Exec=fcitx5 -d
X-GNOME-Autostart-enabled=true
NoDisplay=true
EOF
            ok "XDG autostart entry created"
        else
            ok "XDG autostart entry already exists"
        fi
        ;;
    x11)
        warn "X11 session — add 'fcitx5 -d' to your ~/.xprofile or DE autostart"
        ;;
    *)
        warn "Compositor not detected. Start Fcitx5 manually or add to autostart: fcitx5 -d"
        ;;
esac

# ── Step 6: Start Fcitx5 ──────────────────────────────────────
step 6 "Starting Fcitx5..."

fcitx5 -d 2>/dev/null &
sleep 1

if pgrep -x fcitx5 &>/dev/null; then
    ok "Fcitx5 is running"
else
    warn "Fcitx5 did not start. Run it manually: fcitx5 -d"
fi

# ── Step 7: Verification ──────────────────────────────────────
step 7 "Verifying installation..."

# Check the addon .so was installed.
ADDON_PATH=$(find /usr/lib /usr/lib64 -name 'linuxcomplete.so' 2>/dev/null | head -1)
if [ -n "$ADDON_PATH" ]; then
    ok "Addon installed at $ADDON_PATH"
else
    warn "Could not locate linuxcomplete.so in /usr/lib or /usr/lib64"
fi

# Check data files.
for d in dict frequency ngram rules emoji; do
    if [ -d "/usr/share/linuxcomplete/$d" ]; then
        count=$(find "/usr/share/linuxcomplete/$d" -type f | wc -l)
        ok "Data: $d ($count files)"
    else
        warn "Missing /usr/share/linuxcomplete/$d"
    fi
done

# Check fcitx5 picked up the addon.
if command -v fcitx5-diagnose &>/dev/null; then
    if fcitx5-diagnose 2>/dev/null | grep -q "linuxcomplete"; then
        ok "Fcitx5 detected SmartComplete addon"
    else
        warn "Fcitx5 didn't list SmartComplete — try: killall fcitx5 && fcitx5 -d"
    fi
fi

# ── Done ─────────────────────────────────────────────────────
echo ""
echo -e "${GREEN}${BOLD}  ┌─────────────────────────────────────────┐${NC}"
echo -e "${GREEN}${BOLD}  │        Installation Complete!            │${NC}"
echo -e "${GREEN}${BOLD}  └─────────────────────────────────────────┘${NC}"
echo ""
echo -e "  ${BOLD}Usage:${NC}"
echo -e "    ${CYAN}Tab${NC}         Accept suggestion"
echo -e "    ${CYAN}Up/Down${NC}     Navigate candidates"
echo -e "    ${CYAN}Space${NC}       Commit word + next-word prediction"
echo -e "    ${CYAN}Esc${NC}         Dismiss suggestions"
echo -e "    ${CYAN}:smile${NC}      Emoji shortcodes"
echo ""
if [ -n "$SHELL_RC" ]; then
    echo -e "  ${YELLOW}Next step:${NC} run ${BOLD}source $SHELL_RC${NC} or restart your terminal/session"
    echo -e "  so apps launched later pick up the IM environment variables."
fi
echo ""
echo -e "  To uninstall: ${BOLD}./uninstall.sh${NC}"
echo ""
