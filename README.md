# Clevo RGB Control

An open-source C++ application for controlling the keyboard backlight on Clevo-based laptops.

## Project status

The first Linux CLI implementation sets keyboard RGB color and brightness through the LED class sysfs interface. No effects or support beyond the verified control path are implemented yet.

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

The CLI writes only these LED class attributes; it does not write unknown embedded-controller values. Writes require permission to the relevant sysfs attributes. The observed files are root-owned, so use `sudo` for now. Effects and a normal-user permission mechanism are not implemented.

## Building

Requires CMake and a C++17 compiler:

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

Run `./build/clevo-rgb --help` for options. `--device-dir PATH` selects another LED sysfs directory and can also point to a temporary directory for testing.
