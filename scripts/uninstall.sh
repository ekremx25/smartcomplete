#!/usr/bin/env bash
# SmartComplete — One-Click Uninstaller

set -Eeuo pipefail

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
BLUE='\033[0;34m'; CYAN='\033[0;36m'; BOLD='\033[1m'; NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

step() { echo -e "${BLUE}•${NC} ${BOLD}$1${NC}"; }
ok()   { echo -e "    ${GREEN}✓${NC} $1"; }
warn() { echo -e "    ${YELLOW}!${NC} $1"; }

echo -e "${CYAN}${BOLD}SmartComplete — Uninstaller${NC}"
echo ""

# Keep user data by default. Pass --purge to remove learned words/config.
PURGE=false
if [ "${1:-}" = "--purge" ]; then
    PURGE=true
fi

# ── Stop Fcitx5 ─────────────────────────────────────────
step "Stopping Fcitx5..."
killall fcitx5 2>/dev/null || true
sleep 0.3
ok "Fcitx5 stopped"

# ── Remove installed files via CMake manifest if available ─
step "Removing installed files..."

MANIFEST="$PROJECT_DIR/build/install_manifest.txt"
if [ -f "$MANIFEST" ]; then
    # Remove each file listed by CMake, then prune empty dirs.
    while IFS= read -r f; do
        if [ -n "$f" ] && [ -e "$f" ]; then
            sudo rm -f -- "$f" 2>/dev/null || true
        fi
    done < "$MANIFEST"
    ok "Removed files listed in install_manifest.txt"
else
    # Fallback: remove known install locations manually.
    sudo rm -f /usr/lib/fcitx5/linuxcomplete.so 2>/dev/null || true
    sudo rm -f /usr/lib64/fcitx5/linuxcomplete.so 2>/dev/null || true
    sudo rm -rf /usr/share/linuxcomplete 2>/dev/null || true
    sudo rm -f /usr/share/fcitx5/addon/linuxcomplete.conf 2>/dev/null || true
    sudo rm -f /usr/share/fcitx5/inputmethod/linuxcomplete.conf 2>/dev/null || true
    ok "Removed files from known install locations"
fi

# ── Remove Fcitx5 profile if it points to linuxcomplete ────
step "Cleaning Fcitx5 profile..."

PROFILE="$HOME/.config/fcitx5/profile"
if [ -f "$PROFILE" ] && grep -q "linuxcomplete" "$PROFILE"; then
    cp "$PROFILE" "$PROFILE.bak.$(date +%s)"
    # Replace DefaultIM=linuxcomplete with keyboard-us and remove the linuxcomplete item.
    sed -i '/^Name=linuxcomplete$/,/^$/d' "$PROFILE"
    sed -i 's/^DefaultIM=linuxcomplete$/DefaultIM=keyboard-us/' "$PROFILE"
    ok "Fcitx5 profile cleaned (backup kept)"
else
    ok "Fcitx5 profile has no SmartComplete reference"
fi

# ── Remove autostart entries ───────────────────────────────
step "Removing autostart entries..."

remove_autostart_from() {
    local f="$1"
    [ -f "$f" ] || return 0
    if grep -q "SmartComplete" "$f" 2>/dev/null; then
        # Remove the comment line and the following exec line.
        sed -i '/SmartComplete/,+1d' "$f"
        ok "Cleaned $f"
    fi
    return 0
}

remove_autostart_from "$HOME/.config/hypr/execs.conf"
remove_autostart_from "$HOME/.config/hypr/hyprland.conf"
remove_autostart_from "$HOME/.config/niri/config.kdl"
remove_autostart_from "$HOME/.config/sway/config"

if [ -f "$HOME/.config/autostart/fcitx5.desktop" ] &&
   grep -q "SmartComplete" "$HOME/.config/autostart/fcitx5.desktop" 2>/dev/null; then
    rm -f "$HOME/.config/autostart/fcitx5.desktop"
    ok "Removed XDG autostart entry"
fi

# ── Clean shell RC ─────────────────────────────────────────
step "Cleaning shell RC environment block..."

for rc in "$HOME/.zshrc" "$HOME/.bashrc"; do
    [ -f "$rc" ] || continue
    if grep -q "SmartComplete — Input Method" "$rc"; then
        cp "$rc" "$rc.bak.$(date +%s)"
        # Remove the block: comment line + 5 export lines.
        sed -i '/^# SmartComplete — Input Method Configuration$/,+5d' "$rc"
        ok "Cleaned $rc (backup kept)"
    fi
done

# ── User data (optional) ───────────────────────────────────
if $PURGE; then
    step "Purging user data (--purge)..."
    rm -rf "$HOME/.local/share/linuxcomplete"
    rm -rf "$HOME/.config/linuxcomplete"
    ok "Removed learned words and user config"
else
    warn "User data preserved at ~/.local/share/linuxcomplete and ~/.config/linuxcomplete"
    warn "Re-run with ${BOLD}--purge${NC} to delete them."
fi

echo ""
echo -e "${GREEN}${BOLD}Uninstall complete.${NC}"
echo -e "Restart your session or run ${BOLD}source ~/.bashrc${NC} / ${BOLD}source ~/.zshrc${NC} to reset IM env vars."
