#!/bin/sh
# Publish the factory image of a UART coordinator's C6 as a GitHub release
# asset, where its Olimex fetches it ("Update C6", c6_firmware_url).
#   tools/publish_c6_firmware.sh zb-coord-uart-02.yaml
# The release tag is the part after "zb-coord-" (here uart-02); the device
# file of its host points c6_firmware_url at
#   https://github.com/whulshof/esphome-kasteel/releases/download/<tag>/firmware.factory.bin
# Compile the device file first (Device Builder or esphome compile). The
# repository is public, so the image is too: this refuses an image that
# contains a value from secrets.yaml.
set -e
cd "$(dirname "$0")/.."
DEVICE="${1:?usage: $0 zb-coord-uart-NN.yaml}"
[ -f "$DEVICE" ] || { echo "no device file: $DEVICE"; exit 1; }
NAME="$(sed -n 's/^ *devicename: *"\{0,1\}\([^" #]*\)"\{0,1\}.*/\1/p' "$DEVICE" | head -1)"
TAG="$(basename "$DEVICE" .yaml | sed 's/^zb-coord-//')"
IMAGE=".esphome/build/$NAME/build/firmware.factory.bin"
REPO="whulshof/esphome-kasteel"
[ -n "$NAME" ] || { echo "no devicename in $DEVICE"; exit 1; }
[ -f "$IMAGE" ] || { echo "no image: $IMAGE (compile $DEVICE first)"; exit 1; }

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
    --notes "Factory image of $DEVICE (ESP32-C6 Zigbee coordinator over UART), fetched by its Olimex to update the C6. Replaced on every publish."
fi
rm -rf "$TMP"
echo "published $DEVICE: https://github.com/$REPO/releases/download/$TAG/firmware.factory.bin"
