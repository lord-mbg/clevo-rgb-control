# Clevo RGB Control

An open-source C++ application for controlling the keyboard backlight on Clevo-based laptops.

## Project status

The Linux CLI controls keyboard RGB color and brightness through the LED class sysfs interface. Single-color breathing, rainbow cycling, and two-color transitions have been confirmed on the target keyboard by the user. Effects now start as detached systemd services by default; color-changing breathing, service replacement/stopping, and restoration have been exercised with temporary LED files and a real systemd user manager.

Named lighting profiles can now be saved, listed, applied, and deleted. Persistence and foreground/background application have been exercised with temporary configurations/LED files and a real systemd user manager. The user confirmed that saving a profile as the normal user and applying it to the target keyboard with `sudo --profile` works.

The Qt Quick desktop panel provides independent transition start/end colors, animated previews for all effect modes at the selected speed and brightness, and profile management. Selecting a GUI-compatible profile loads its colors, brightness, effect, period, and preview into the editor without touching the keyboard. Profiles with CLI-only palettes or durations remain applicable through **Profili Uygula**, but are not loaded into the editor. The panel also polls the detached systemd service every five seconds and shows whether an effect is running; status-query failures remain explicitly unknown. GUI interactions have been exercised against temporary LED/configuration files. The desktop controller's `pkexec` path has also been exercised on the physical keyboard with a temporary Qt driver: a replacement transition was started and stopped, its prior LED values were restored, and the original effect was restarted with its original arguments. This does not establish support for other devices.

## Initial target

- Laptop: Monster/Clevo ABRA A5 V17.3
- Platform: Linux
- Interface: command-line tool
- Controls: keyboard RGB color and brightness

## Hardware interface

On the verified machine, the TUXEDO `tuxedo_keyboard` driver exposes the keyboard LED at `/sys/class/leds/rgb:kbd_backlight`:

- RGB channels, in red/green/blue order: `multi_intensity`
- Brightness: `brightness`
- Brightness limit: `max_brightness`

The CLI writes only these LED class attributes; it does not write unknown embedded-controller values. Writes require permission to the relevant sysfs attributes. The observed files are root-owned, so CLI writes use `sudo`; the desktop panel invokes the CLI through `pkexec`. Direct normal-user sysfs permissions are not configured.

## Building

Requires CMake and a C++17 compiler. Qt 6.5 or newer with Quick, QuickControls2, and Widgets development files enables the optional desktop target; Qt Test enables the GUI interaction regression. The runtime also needs the Qt Quick Controls Basic, Layouts, Dialogs, and Shapes QML modules. If Qt is unavailable, CMake builds only the CLI. To explicitly disable the desktop target, configure with `-DBUILD_GUI=OFF`.

Detached operation also requires a running systemd manager with `systemd-run` and `systemctl`. Root invocations use the system manager; ordinary users use their user manager. `--foreground` runs an effect without systemd. The executable must remain at its original path for future service launches; no effect service installation or boot-time restart is performed.

```sh
cmake -S . -B build
cmake --build build
```

## Desktop panel

```sh
./build/clevo-rgb-ui
```

Run the panel as the normal desktop user, **not with sudo**. Keep `clevo-rgb-ui` and `clevo-rgb` together in the build directory. The panel reads LED state on startup; **Yenile** rereads it without replacing unsaved editor settings. It separately queries the matching systemd scope for a detached effect at startup, after operations, on refresh, and about every five seconds. **Efekt çalışıyor**, **Efekt yok**, and **Efekt bilinmiyor** distinguish running, inactive, and unavailable/failed status queries. The keyboard illustration previews the chosen mode, colors, speed, and brightness, not live device output.

- Editing controls does not write hardware. **Uygula** applies the chosen color/brightness or starts the selected effect. In **Geçiş** mode the start and end colors are independently editable in the color card. The period is a complete effect cycle in milliseconds.
- Selecting a saved profile loads supported settings into the editor and preview; the keyboard is unchanged until **Profili Uygula** or **Uygula**. Profiles using CLI-only palettes or finite durations are not loaded into the editor (a status message explains why); **Profili Uygula** still applies their original settings.
- **Efekti Durdur** stops the detached worker and restores its initial values. Closing the panel does not stop an already detached effect.
- With a system tray, closing the window hides it; the tray icon or **Göster/Gizle** menu restores/hides it, and **Çıkış** quits. If no tray is available when starting, closing the window quits normally.
- Sysfs actions use `pkexec`, so Polkit and an active desktop authentication agent are required. Authorization may prompt for a password; cancellation and CLI failures appear in the status area. Hover over the status text to read the full message.
- Profile save/list/delete run as the desktop user, with the same storage and validation as the CLI. Existing-profile overwrite and deletion require confirmation. Applying a profile resolves that user's settings before elevation; it does not depend on `SUDO_UID` or root's configuration.
- No device is required to save or manage profiles. Device-dependent apply buttons are disabled if the LED attributes cannot be read. **Efekti Durdur** remains available when status is unknown, but is disabled when no managed effect is running.

For a hardware-free desktop run, point `--device-dir` at a temporary directory containing `multi_intensity`, `brightness`, and `max_brightness`, as in the validation example below. Writable temporary directories run CLI operations without elevation under the systemd user manager:

```sh
./build/clevo-rgb-ui --device-dir /path/to/temporary-led-directory
```

### Starting with the desktop session

An XDG autostart entry launches the panel as the signed-in user, without touching the keyboard or requesting privilege at login. On this workstation, `~/.config/autostart/clevo-rgb-ui.desktop` points to the built panel. For another installation, use its own absolute executable path in both fields:

```ini
[Desktop Entry]
Type=Application
Name=Clevo RGB Control
Exec=/absolute/path/to/clevo-rgb-control/build/clevo-rgb-ui
TryExec=/absolute/path/to/clevo-rgb-control/build/clevo-rgb-ui
Terminal=false
```

This starts the panel at graphical login, **not** the RGB effect at boot. Effects are transient systemd units; a reboot does not restart them. Automatically applying a hardware profile would require a separate privileged setup and must not bypass Polkit. Keep other lighting software from writing the same LED simultaneously; do not disable a whole power-management daemon solely to change its keyboard behavior.

## Usage

Set color (RGB channels are 0–255):

```sh
sudo ./build/clevo-rgb --color 84 106 202
```

Set brightness (the CLI reads the device's `max_brightness` and checks the requested range):

```sh
sudo ./build/clevo-rgb --brightness 128
```

Set both:

```sh
sudo ./build/clevo-rgb --color 255 0 0 --brightness 128
```

## Profiles

Save lighting settings without changing the keyboard or starting a service:

```sh
# Save as the normal user (no sudo).
./build/clevo-rgb --save-profile calisma --effect breathe \
  --color 0 0 255 --to-color 128 0 255 --brightness 180 --period-ms 6000

./build/clevo-rgb --save-profile gece --color 84 106 202 --brightness 40

./build/clevo-rgb --list-profiles

# Apply the stored settings; an effect starts detached, a static profile writes once.
sudo ./build/clevo-rgb --profile calisma
sudo ./build/clevo-rgb --profile gece

sudo ./build/clevo-rgb --stop
./build/clevo-rgb --delete-profile gece
```

- Profiles store effect, explicit color/target or breathing palette, optional brightness, period, and optional duration. Omitted brightness remains device-dependent at application time; saving does not read the current keyboard state.
- Saving the same name replaces its complete settings atomically. Invalid settings leave the previous record intact.
- Names: 1–64 ASCII letters/digits/`-`/`_`, starting with a letter or digit. Use names such as `calisma`, not filesystem paths.
- Storage: `$XDG_CONFIG_HOME/clevo-rgb/profiles/` when that variable is an absolute path; otherwise `$HOME/.config/clevo-rgb/profiles/`. Files are private (`0600`), with a versioned plain-text lighting format. Symlinked profile files/directories and wrong owners are rejected.
- `sudo --profile` resolves the invoking user's home via `SUDO_UID`, not root's `HOME`. Save/delete must run without sudo. If using a custom XDG directory, preserve `XDG_CONFIG_HOME` explicitly when applying with sudo; otherwise the user's default `.config` directory is used. Direct root use without a non-root `SUDO_UID` uses root's own store.
- `--profile NAME` permits `--device-dir PATH` and `--foreground` (effects only), not lighting overrides. To change a profile, save it again with the complete desired settings.
- Device paths, foreground/background mode, and management commands are never stored. Detached workers receive resolved lighting arguments; changing or deleting a profile does not change an already running effect.

## Effects

This keyboard has one RGB zone: all keys share the same color and brightness. These effects change the whole keyboard over time, not individual keys or spatial regions.

```sh
# Breathe through the built-in palette; change colors while dark.
sudo ./build/clevo-rgb --effect breathe --brightness 200

# Alternate blue and red, one color per breath.
sudo ./build/clevo-rgb --effect breathe --color 0 0 255 --to-color 255 0 0 --brightness 200

# Custom breathing sequence: blue, violet, cyan, repeat.
sudo ./build/clevo-rgb --effect breathe --palette 0 0 255 --palette 128 0 255 --palette 0 255 255

# Explicit --color without --to-color keeps breathing in one color.
sudo ./build/clevo-rgb --effect breathe --color 84 106 202 --brightness 200

# Smoothly cycle through the rainbow.
sudo ./build/clevo-rgb --effect cycle --period-ms 6000

# Fade from red to blue and back.
sudo ./build/clevo-rgb --effect transition --color 255 0 0 --to-color 0 0 255

# Run for ten seconds, then restore the previous settings.
sudo ./build/clevo-rgb --effect cycle --duration-ms 10000

# Stop the background effect and restore its initial color/brightness.
sudo ./build/clevo-rgb --stop
```

- Commands return after the detached service has initialized its device. The terminal may then be closed. One managed effect exists per system/user scope; a new effect synchronously stops and restores the previous one before starting. Static color/brightness commands also stop the managed effect before applying their settings. Keep the same privilege scope for start and stop (`sudo` on the target keyboard).
- `--foreground`: opt into attached operation for debugging; Ctrl+C stops and restores it. Do not run foreground and managed effects concurrently on the same device.
- `--period-ms`: one full breath, rainbow tour, or two-color round trip; default 3000, allowed range 100–3600000. A multi-color breathing palette advances one color per breath.
- `--duration-ms`: positive integer milliseconds; omitted means run until `--stop` (or Ctrl+C in foreground mode).
- `--palette R G B`: repeat for 2–64 colors, only for breathing; cannot combine with `--color`/`--to-color`. Bare `--effect breathe` uses the built-in palette. Color changes are preceded by an explicit zero-brightness write, even if scheduling skips the exact end of a breath.
- `--brightness`: constant brightness for cycle/transition, peak brightness for breathing. Defaults to the initial brightness; if it is zero, specify a nonzero value to see the effect.
- Updates are capped at 25 frames/second, with unchanged values skipped.
- The worker remains running under systemd, not the terminal. Duration expiry, `--stop`, and SIGTERM restore its initial RGB and brightness. Write errors also attempt restoration and return failure; inspect `journalctl -u clevo-rgb-effect.service` (add `--user` for user scope). SIGKILL, power loss, or an inaccessible device cannot guarantee restoration. User services may stop at logout according to the system's session policy; closing a terminal is not boot persistence.
- Effects require readable color, brightness, and maximum brightness, and writable color and brightness. Avoid concurrent lighting controllers or active LED triggers; the CLI does not change `trigger`.

### Color selection and bounded transitions

`./build/clevo-rgb --list-colors` lists 13 coarse nominal samples: 12 fully saturated hue steps plus white. Use their numeric values with `--color`, `--to-color`, or repeated `--palette` arguments.

The 8-bit RGB interface accepts 256³ = 16,777,216 combinations, including black/off. This is a count of numeric inputs, not a measured number of distinct visible keyboard colors. LED response, channel balance, brightness, keycaps, and vision affect the result; adjacent numeric values need not look different. The coarse palette is a starting point for visual comparison at a fixed brightness, not a calibrated hardware gamut or exhaustive list.

`cycle` still traverses the full rainbow. To stay between selected RGB endpoints, use `transition`: it interpolates their channels and returns, without traversing the rest of the rainbow. This is an RGB blend, not a selectable hue arc; for example red-to-blue passes through darker purple, not green.

### Validation

Build with the commands above. For a hardware-free smoke run, create temporary LED files:

```sh
device=$(mktemp -d)
printf '23 45 67\n' > "$device/multi_intensity"
printf '80\n' > "$device/brightness"
printf '255\n' > "$device/max_brightness"
./build/clevo-rgb --device-dir "$device" --foreground --effect cycle --duration-ms 1000
cat "$device/multi_intensity" "$device/brightness"
# Expected restored values: 23 45 67, then 80.
rm -r "$device"
```

Run `./build/clevo-rgb --help` for options. `--device-dir PATH` selects another LED sysfs directory and can also point to a temporary directory for testing.

With Python 3 available at CMake configuration time, the isolated profile boundary regression is registered. With Qt Test available, the GUI interaction regression also runs with a real controller, QML, CLI, and temporary LED/profile files; its systemd query is isolated so it cannot stop a user's service:

```sh
ctest --test-dir build --output-on-failure
```

These checks do not write to the physical keyboard. Physical-device Polkit verification is a separate interactive operation requiring desktop authentication; the recorded hardware test above was performed outside the automated suite.
