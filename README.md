# Trad Auto-DS4 Controller

**Trad Auto-DS4 Controller** (branded under **TRAD** for GitHub user **KidDzhasi**) is a native Linux desktop GUI application built with **C++17** and **Qt 6** (`CMake`) for **Fedora Workstation 45 (GNOME / Wayland)**.

It manages Bluetooth detection, pairing, and white-LED stall recovery for **Generic DualShock 4 / Doubleshock 4 (G-Controller)** and **Original Sony DualShock 4** gamepads (defaulting to MAC address `89:56:AB:5D:AC:8E`).

---

## Features

1. **Auto-Refreshing Status Dashboard**:
   - Automatically polls and displays controller state (`Paired`, `Trusted`, `Connected`, `Gamepad HID`), controller classification badge (`Generic (G-Controller)` vs `Original (Sony DS4)`), and active default MAC address every 3 seconds.
2. **Scan Controller (Generic vs Original Detection)**:
   - Scans Bluetooth devices (`bluetoothctl --timeout 8 scan on`), detects gamepads, and classifies whether the controller is **Generic (`G-Controller`)** or **Original (`Sony DS4`)** based on IEEE OUI and BlueZ BR/EDR bonding characteristics.
   - When a **Generic** controller is detected, its MAC address is automatically saved as the default target MAC in `~/.config/trad-auto-ds4-controller/config.ini` until the next scan.
3. **Connect G-Controller**:
   - Executes the proven `repair-ds4` workflow: removes any stale `<MAC>` entry (`bluetoothctl remove <MAC>`), prompts you to hold **SHARE + Home (PS)** until the light bar double-flashes, scans for 10 seconds, and runs `pair`, `trust`, and `connect`.
4. **Disconnect Action**:
   - Cleanly disconnects the active controller via `bluetoothctl disconnect <MAC>`.
5. **Live Activity Log**:
   - Streams non-blocking `QProcess` output in real time without freezing the Qt event loop.

---

## Fedora Workstation 45 Dependencies

Install the required build and runtime packages on Fedora 45:

```bash
sudo dnf install -y gcc-c++ cmake qt6-qtbase-devel qt6-qtsvg bluez desktop-file-utils
```

> **Note for Generic DS4 Controllers on Fedora 45 (BlueZ 5.88+)**:
> Generic DualShock 4 controllers often connect with `BREDR.Bonded: no`. Ensure `/etc/bluetooth/input.conf` has `ClassicBondedOnly=false` enabled (and restart `bluetooth.service`: `sudo systemctl restart bluetooth`) so `bluetoothd` attaches the kernel `hid_playstation` input driver instead of rejecting non-bonded HID connections.

---

## Build & Install

Run the included `install.sh` script from the project root:

```bash
chmod +x install.sh
./install.sh
```

What `install.sh` installs:
- GUI Binary: `~/.local/bin/trad-auto-ds4-controller` (and `~/.local/bin/trad-repair-ds4` symlink)
- CLI Helper: `~/.local/bin/repair-ds4`
- App Icon: `~/.local/share/icons/hicolor/scalable/apps/trad-auto-ds4-controller.svg`
- Desktop Entry: `~/.local/share/applications/trad-auto-ds4-controller.desktop`
