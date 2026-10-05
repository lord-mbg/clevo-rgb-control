# Clevo RGB Control

An open-source C++ application for controlling the keyboard backlight on Clevo-based laptops.

## Project status

The Linux CLI controls keyboard RGB color and brightness through the LED class sysfs interface. Single-color breathing, rainbow cycling, and two-color transitions have been confirmed on the target keyboard by the user. Effects now start as detached systemd services by default; color-changing breathing, service replacement/stopping, and restoration have been exercised with temporary LED files and a real systemd user manager.

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

The CLI writes only these LED class attributes; it does not write unknown embedded-controller values. Writes require permission to the relevant sysfs attributes. The observed files are root-owned, so use `sudo` for now. A normal-user permission mechanism is not implemented.

## Building

Requires CMake and a C++17 compiler:

Detached operation also requires a running systemd manager with `systemd-run` and `systemctl`. Root invocations use the system manager; ordinary users use their user manager. `--foreground` runs an effect without systemd. The executable must remain at its original path for future service launches; no service installation or boot autostart is performed.

```sh
cmake -S . -B build
cmake --build build
```

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

## Planned: camera-measured color palette

Agreed plan; not implemented yet. Tilt the laptop lid so its camera can see the illuminated keyboard, then automate RGB changes and capture images to build a practical measured palette.

1. Confirm that a sample image resolves illuminated key legends. Fix camera/lid position, keyboard brightness, and ambient lighting; keep screen illumination out of the measurement.
2. Lock camera exposure, gain, and white balance where supported; avoid clipped highlights. Select a fixed illuminated-key region rather than averaging the whole image.
3. Capture repeated frames at the same RGB setting to estimate measurement noise. Include black/off as an ambient reference.
4. Start with a coarse three-dimensional RGB grid, for example channel levels 0, 64, 128, 192, and 255 (125 combinations). Wait for the image to settle after each change, then summarize several frames robustly.
5. Compare measured colors using a color-distance metric such as Lab/Delta E. Subdivide intervals with differences reliably above the noise floor; also sample intermediate points to check for missed changes. Stop at the noise floor or an input step of one.
6. Cluster similar measured results and retain representative RGB inputs for effect palettes. Save capture settings and RGB-to-measurement associations so results can be reproduced.

The result describes colors distinguishable by this camera under these conditions, not the keyboard's absolute color count or a calibrated human-perception gamut. Visually compare representative samples before adopting the palette.
