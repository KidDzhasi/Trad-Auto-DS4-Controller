#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=== Building Trad Auto-DS4 Controller (TRAD / KidDzhasi) ==="
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"

echo "=== Installing binary to ~/.local/bin ==="
mkdir -p "$HOME/.local/bin"
install -m 755 build/trad-auto-ds4-controller "$HOME/.local/bin/trad-auto-ds4-controller"
ln -sf "$HOME/.local/bin/trad-auto-ds4-controller" "$HOME/.local/bin/trad-repair-ds4"

# Install standalone repair-ds4 CLI script if not present or ensure it is executable
cat <<'EOF' > "$HOME/.local/bin/repair-ds4"
#!/usr/bin/env bash
set -euo pipefail

MAC="${1:-89:56:AB:5D:AC:8E}"

echo "Removing existing entry for $MAC..."
bluetoothctl remove "$MAC" >/dev/null 2>&1 || true

echo "Put the controller into pairing mode (hold SHARE + Home until the light double-flashes), then press Enter..."
read -r

echo "Scanning for $MAC..."
bluetoothctl --timeout 10 scan on >/dev/null 2>&1 || true

echo "Pairing..."
bluetoothctl pair "$MAC"

echo "Trusting..."
bluetoothctl trust "$MAC"

echo "Connecting..."
bluetoothctl connect "$MAC"

echo "Checking status..."
bluetoothctl info "$MAC" | grep -E "Name|Paired|Trusted|Connected"
EOF
chmod +x "$HOME/.local/bin/repair-ds4"

echo "=== Installing SVG icon & Desktop Entry to ~/.local/share ==="
mkdir -p "$HOME/.local/share/icons/hicolor/scalable/apps"
install -m 644 assets/trad-auto-ds4-controller.svg "$HOME/.local/share/icons/hicolor/scalable/apps/trad-auto-ds4-controller.svg"
install -m 644 assets/trad-auto-ds4-controller.svg "$HOME/.local/share/icons/hicolor/scalable/apps/trad-repair-ds4.svg"

mkdir -p "$HOME/.local/share/applications"
install -m 644 packaging/trad-auto-ds4-controller.desktop "$HOME/.local/share/applications/trad-auto-ds4-controller.desktop"

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$HOME/.local/share/applications" || true
fi

if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t "$HOME/.local/share/icons/hicolor" >/dev/null 2>&1 || true
fi

echo ""
echo "Installation complete!"
echo "  GUI Binary:    $HOME/.local/bin/trad-auto-ds4-controller"
echo "  CLI Script:    $HOME/.local/bin/repair-ds4"
echo "  Desktop entry: $HOME/.local/share/applications/trad-auto-ds4-controller.desktop"
echo "  Icon:          $HOME/.local/share/icons/hicolor/scalable/apps/trad-auto-ds4-controller.svg"
