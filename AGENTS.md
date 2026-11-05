# Repository Guidelines

## Project Overview

Clevo RGB Control is a Linux C++17 CLI for RGB keyboard backlighting on the Monster/Clevo ABRA A5 V17.3, plus an optional Qt Quick desktop panel. The documented hardware interface is the TUXEDO `tuxedo_keyboard` LED class device; do not claim support for other devices or untested authorization paths.

## Architecture & Safety

The CLI in `src/main.cpp`, `src/effects.cpp`, `src/service.cpp`, and `src/profiles.cpp` validates lighting requests, writes only verified LED class attributes, and manages detached effects through systemd. The desktop UI in `src/ui/` invokes the sibling CLI via `QProcess`; hardware operations requiring privilege use `pkexec`. Never write unknown embedded-controller values directly. Use a temporary `--device-dir` for hardware-free validation, not the physical keyboard.

## Files & Commands

- `CMakeLists.txt` configures the CLI, optional Qt 6.5+ GUI (`BUILD_GUI`), Python profile-boundary tests, and optional Qt Test GUI interaction tests.
- `README.md` documents verified behavior, build/runtime prerequisites, and hardware-free smoke instructions.
- `tests/test_profiles.py` and `tests/gui_interactions.cpp` exercise profile safety and actual QML/controller/CLI interactions on temporary LED files.
- `.gitignore` excludes build outputs and native artifacts.
- Build: `cmake -S . -B build && cmake --build build`.
- Test: `ctest --test-dir build --output-on-failure`.
- Hardware operations should use the root-owned CLI installed beside the GUI in `/opt/clevo-rgb-control/`; never elevate the user-writable `build/` copy. Rebuild, test, then install both binaries and restart the panel when updating.
- To build without Qt: configure with `-DBUILD_GUI=OFF`.

## Conventions & Verification

Keep UI controls separate from OS-specific hardware access. Preserve the CLI's validation and secure profile-storage boundaries; profile writes must not contain device paths. Verify changed behavior on temporary LED files; GUI interactions should be checked in an actual Qt session when available. Never infer physical-device support or Polkit authorization success from a temporary-file run.
