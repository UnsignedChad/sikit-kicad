#!/usr/bin/env bash
# Symlink the built plugin into kicad 9's plugin dir so pcbnew picks it
# up on next start. Enable the API server first:
#   Preferences > Plugins > Enable KiCad API  (or set
#   api.enable_server=true in ~/.config/kicad/$kv/kicad_common.json)
set -euo pipefail
# settings/data dirs are versioned per kicad minor (9.0, 10.0, ...)
kv="$(kicad-cli version | cut -d. -f1,2)"
here="$(cd "$(dirname "$0")/.." && pwd)"
dest="$HOME/.local/share/kicad/$kv/plugins/com.unsignedchad.sikit"

[ -x "$here/build/sikit-kicad" ] || {
    echo "build first: cmake -B build -G Ninja && cmake --build build" >&2
    exit 1
}
mkdir -p "$dest"
ln -sf "$here/plugin.json" "$dest/plugin.json"
ln -sf "$here/build/sikit-kicad" "$dest/sikit-kicad"
ln -sfn "$here/resources" "$dest/resources"
echo "installed: $dest"
