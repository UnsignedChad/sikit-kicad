# sikit-kicad

KiCad plugin that opens the board you have in pcbnew straight in
[sikit](https://github.com/UnsignedChad/circuitcore) for signal
integrity work: trace impedance, S-parameters, eye diagrams,
crosstalk, return-path checks.

No physics lives here. The engines, widgets and the kicad file parser
come from circuitcore, pinned via FetchContent. This repo is the
KiCad-facing shell: an exec-type IPC plugin (KiCad 9+) plus a thin
nng/protobuf client.

## how it works

KiCad launches `sikit-kicad` with `KICAD_API_SOCKET` and
`KICAD_API_TOKEN` set. The plugin asks the running KiCad for the open
PCB over the IPC API (protobuf over nng), loads that board through
circuitcore's parser, and shows sikit's main window. Run it without a
KiCad around and it behaves like plain sikit (optional board path
argument).

Live item sync and selection-aware analysis are roadmap; v1 reads the
board file from disk, so save before analyzing.

## build

Needs Qt6, Eigen3, spdlog, libnng-dev, libprotobuf-dev,
protobuf-compiler. circuitcore is fetched automatically.

    cmake -B build -G Ninja
    cmake --build build

## install from the plugin manager

Open KiCad > Plugin and Content Manager > Manage..., add

    https://raw.githubusercontent.com/UnsignedChad/sikit-kicad/main/pcm/repository.json

then pick the "sikit (UnsignedChad)" repository and install sikit.
Enable the API server (Preferences > Plugins) and restart pcbnew; the
sikit button shows up at the right end of the top toolbar.

KiCad 9 on Linux does not preserve the executable bit when it extracts
packages, so after install run:

    chmod +x ~/.local/share/kicad/9.0/3rdparty/plugins/com_unsignedchad_sikit/sikit-kicad

KiCad 10 sets it automatically (pcm extraction restores zip modes).

## developer install (from a checkout)

    scripts/install-local.sh

symlinks the build into the user plugins dir; same API-server +
restart notes apply.

## test

    scripts/itest.sh   # spins up pcbnew under xvfb, probes, gui-smokes

`sikit-kicad --probe` against a running KiCad prints the version and
the open boards.

## license

GPL-3.0-or-later. `proto/` is vendored from KiCad 9.0.8 (GPLv3).
