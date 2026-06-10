#!/usr/bin/env bash
# End-to-end test against a real pcbnew under xvfb: enable the api
# server, open a board, then drive the plugin through probe + gui-smoke.
set -euo pipefail
# settings/data dirs are versioned per kicad minor (9.0, 10.0, ...)
kv="$(kicad-cli version | cut -d. -f1,2)"
here="$(cd "$(dirname "$0")/.." && pwd)"
board="${1:-$here/build/_deps/circuitcore-src/pdnkit/tests/fixtures/tiny_pdn.kicad_pcb}"
cfg="$HOME/.config/kicad/$kv/kicad_common.json"
sock_path="/tmp/kicad/api.sock"

[ -f "$board" ] || { echo "no board at $board" >&2; exit 1; }
[ -x "$here/build/sikit-kicad" ] || { echo "build first" >&2; exit 1; }

python3 - "$cfg" <<'PY'
import json, sys
p = sys.argv[1]
d = json.load(open(p))
d.setdefault("api", {})["enable_server"] = True
json.dump(d, open(p, "w"), indent=2)
print("api.enable_server = true")
PY

rm -f "$sock_path"
xvfb-run -a -s "-screen 0 1280x800x24" pcbnew "$board" &
pcb_pid=$!
cleanup() { kill "$pcb_pid" 2>/dev/null || true; wait "$pcb_pid" 2>/dev/null || true; }
trap cleanup EXIT

echo "waiting for $sock_path ..."
for _ in $(seq 1 60); do
    [ -S "$sock_path" ] && break
    kill -0 "$pcb_pid" 2>/dev/null || { echo "pcbnew died" >&2; exit 1; }
    sleep 1
done
[ -S "$sock_path" ] || { echo "api socket never appeared" >&2; exit 1; }
sleep 2  # let pcbnew finish loading the board

export KICAD_API_SOCKET="ipc://$sock_path"

# kicad serves AS_NOT_READY while the frame is still coming up; poll.
echo "--- probe (retrying until kicad is ready) ---"
ok=0
for _ in $(seq 1 30); do
    if "$here/build/sikit-kicad" --probe; then ok=1; break; fi
    sleep 2
done
[ "$ok" = 1 ] || { echo "kicad never became ready" >&2; exit 1; }

echo "--- gui smoke (offscreen) ---"
QT_QPA_PLATFORM=offscreen "$here/build/sikit-kicad" --smoke-gui

echo "itest ok"
