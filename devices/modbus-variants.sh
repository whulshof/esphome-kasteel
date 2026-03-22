#!/usr/bin/env bash

# Config
DEVICE="192.168.2.8"
PORT="502"


LOG_FILE="yunt_modbus_scan_$(date +%Y%m%d_%H%M%S).log"

scan_block() {
  local slave="$1"
  local FC="$2"      # Modbus function code: 3 = holding, 4 = input
  local START="$3"   # start address (decimaal)
  local COUNT="$4"   # aantal registers
  local LABEL="$5"   # beschrijving

  echo "=== $LABEL (FC$FC, start=$START, count=$COUNT) ===" | tee -a "$LOG_FILE"

  mbpoll -1 \
    -m tcp \
    -a "$SLAVE" \
    -0 \
    -t "$FC" \
    -r "$START" \
    -c "$COUNT" \
    -p "$PORT" \
    "$DEVICE"  | tee -a "$LOG_FILE"

  echo "" | tee -a "$LOG_FILE"
}

echo "Logbestand: $LOG_FILE"
echo "" | tee -a "$LOG_FILE"

# ─────────────────────────────────────────────
# GERICHTE BLOKKEN (op basis van jouw maps)
# Pas gerust aan / uitcommentariëren.
# ─────────────────────────────────────────────

# BMS / batterij hoofdinformatie (0x1100 = 4352 dec)
#scan_block 3 4352 64  "BMS main status (0x1100-blok)"
#
## BMS extra cell/temperatuur/alarm-info (0x1110 = 4368 dec)
#scan_block 3 4368 64  "BMS detail + alarmen (0x1110-blok)"
#
## EMS/PCS commando-gebied rond 0x1000 (stel, power/mode/time etc)
#scan_block 3 4096 64  "EMS / command area (0x1000-blok)"
#
## PCS / AC-side vermogens/spanningen etc. (0x2000 = 8192 dec)
#scan_block 3 8192 64  "PCS / grid-side status (0x2000-blok)"
#
## Eventuele AC-meter rond 0x0170 / 0x18x (voorbeeld; kun je uitzetten als niet aanwezig)
#scan_block 3 368 64   "AC-meter / power & energy (0x0170 / 0x018x-blokken)"
#
## Eventuele stack/cluster-info rond 0x0010–0x0040 (andere BMS-variant)
#scan_block 3 16 64    "Stack-level BMS registers (0x0010-blok)"

echo "Klaar. Resultaat staat in: $LOG_FILE"
