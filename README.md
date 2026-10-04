# Clevo RGB Control

An open-source C++ application for controlling the keyboard backlight on Clevo-based laptops.

## Project status

Planning and Linux hardware-interface research. The application is not implemented yet, and no device support is claimed at this stage.

## Initial target

- Laptop: Monster/Clevo ABRA A5 V17.3
- Platform: Linux
- First interface: command-line tool
- Initial controls: keyboard color and brightness

A desktop interface and support for other operating systems or laptop models may be considered after the initial control path is verified. Untested devices will not be described as supported.

## Design approach

The application will keep its user-facing controls separate from operating-system-specific hardware access. Hardware writes will use a verified control interface; unknown embedded-controller values will not be written directly.

## Building

The project is not buildable yet. Build and usage instructions will be added with the first working implementation.
