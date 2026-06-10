#!/usr/bin/env bash
# Build the PCM package zip + regenerate pcm/packages.json and
# pcm/repository.json. Run after a release build:
#   cmake --build build && scripts/package-pcm.sh 0.1.0
# Upload the zip as a github release asset for tag v<version>; the
# download url in packages.json assumes that location.
set -euo pipefail
here="$(cd "$(dirname "$0")/.." && pwd)"
ver="${1:?usage: package-pcm.sh <version>}"

[ -x "$here/build/sikit-kicad" ] || { echo "build first" >&2; exit 1; }

stage="$(mktemp -d)"
trap "rm -rf $stage" EXIT
mkdir -p "$stage/plugins" "$stage/resources"

cp "$here/plugin.json"        "$stage/plugins/"
cp "$here/build/sikit-kicad"  "$stage/plugins/"
cp -r "$here/resources"       "$stage/plugins/"
cp "$here/resources/icon.png" "$stage/resources/icon.png"

python3 - "$here" "$ver" "$stage" << "PY"
import hashlib, json, os, subprocess, sys, time

here, ver, stage = sys.argv[1], sys.argv[2], sys.argv[3]

meta = {
    "$schema": "https://go.kicad.org/pcm/schemas/v1",
    "name": "sikit",
    "description": "Signal integrity analysis for the open board",
    "description_full": (
        "Opens the board you have in pcbnew straight in sikit: trace "
        "impedance, S-parameters, eye diagrams, crosstalk, return-path "
        "checks. Physics comes from the circuitcore engines. Linux only "
        "for now; needs Qt6, libnng1 and libprotobuf at runtime."),
    "identifier": "com.unsignedchad.sikit",
    "type": "plugin",
    "author": {"name": "Charles Kennedy",
               "contact": {"web": "https://github.com/UnsignedChad"}},
    "license": "GPL-3.0-or-later",
    "resources": {"homepage": "https://github.com/UnsignedChad/sikit-kicad"},
    "tags": ["signal integrity", "analysis", "impedance", "s-parameters"],
}

version_entry = {
    "version": ver,
    "status": "testing",
    "kicad_version": "9.0",
    "platforms": ["linux"],
    "runtime": "ipc",
}

# metadata.json inside the zip: no download fields
meta_zip = dict(meta)
meta_zip["versions"] = [dict(version_entry)]
open(os.path.join(stage, "metadata.json"), "w").write(
    json.dumps(meta_zip, indent=4))

zip_name = f"sikit-kicad-pcm-{ver}-linux-x64.zip"
zip_path = os.path.join(here, "build", zip_name)
if os.path.exists(zip_path):
    os.unlink(zip_path)
subprocess.run(["zip", "-rq", zip_path, "."], cwd=stage, check=True)

sha = hashlib.sha256(open(zip_path, "rb").read()).hexdigest()
dl_size = os.path.getsize(zip_path)
inst_size = 0
for root, _, files in os.walk(stage):
    inst_size += sum(os.path.getsize(os.path.join(root, f)) for f in files)

# packages.json: same package, version entry with download info
version_entry.update({
    "download_url": (
        f"https://github.com/UnsignedChad/sikit-kicad/releases/download/"
        f"v{ver}/{zip_name}"),
    "download_sha256": sha,
    "download_size": dl_size,
    "install_size": inst_size,
})
pkg = dict(meta)
pkg["versions"] = [version_entry]
packages = {"packages": [pkg]}
pkgs_path = os.path.join(here, "pcm", "packages.json")
open(pkgs_path, "w").write(json.dumps(packages, indent=4))

now = int(time.time())
repo = {
    "$schema": "https://go.kicad.org/pcm/schemas/v1",
    "name": "sikit (UnsignedChad)",
    "maintainer": {"name": "Charles Kennedy",
                   "contact": {"web": "https://github.com/UnsignedChad"}},
    "packages": {
        "url": ("https://raw.githubusercontent.com/UnsignedChad/"
                "sikit-kicad/main/pcm/packages.json"),
        "sha256": hashlib.sha256(open(pkgs_path, "rb").read()).hexdigest(),
        "update_timestamp": now,
        "update_time_utc": time.strftime("%Y-%m-%d %H:%M:%S",
                                          time.gmtime(now)),
    },
}
open(os.path.join(here, "pcm", "repository.json"), "w").write(
    json.dumps(repo, indent=4))

print(f"zip: {zip_path}")
print(f"sha256: {sha}")
print("pcm/packages.json + pcm/repository.json regenerated")
PY
