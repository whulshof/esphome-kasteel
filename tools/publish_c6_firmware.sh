#!/bin/sh
# Publish the factory image of the UART coordinator as a GitHub release asset,
# where the Olimex fetches it ("Update C6", c6_firmware_url).
#   tools/publish_c6_firmware.sh [image] [release tag]
# Compile zigbee-coordinator-uart-01.yaml first (Device Builder or esphome
# compile). The repository is public, so the image is too: this refuses an
# image that contains a value from secrets.yaml.
set -e
cd "$(dirname "$0")/.."
IMAGE="${1:-.esphome/build/zb-coord-uart-01-/build/firmware.factory.bin}"
TAG="${2:-uart-01}"
REPO="whulshof/esphome-kasteel"
[ -f "$IMAGE" ] || { echo "no image: $IMAGE (compile first)"; exit 1; }

python3 - "$IMAGE" <<'PY'
import re, sys
image = open(sys.argv[1], "rb").read()
found = []
for line in open("secrets.yaml"):
    m = re.match(r"\s*([A-Za-z0-9_]+)\s*:\s*[\"']?(.+?)[\"']?\s*(#.*)?$", line)
    if m and len(m.group(2)) >= 6 and m.group(2).encode() in image:
        found.append(m.group(1))
if found:
    sys.exit("refused: the image contains secrets: " + ", ".join(found))
PY

TMP="$(mktemp -d)"
cp "$IMAGE" "$TMP/firmware.factory.bin"
if gh release view "$TAG" --repo "$REPO" >/dev/null 2>&1; then
  gh release upload "$TAG" "$TMP/firmware.factory.bin" --repo "$REPO" --clobber
else
  gh release create "$TAG" "$TMP/firmware.factory.bin" --repo "$REPO" --target home \
    --title "Firmware $TAG" \
    --notes "Factory image of zigbee-coordinator-uart-01 (ESP32-C6 Zigbee coordinator over UART), fetched by the Olimex bridge to update the C6. Replaced on every publish."
fi
rm -rf "$TMP"
echo "published: https://github.com/$REPO/releases/download/$TAG/firmware.factory.bin"
