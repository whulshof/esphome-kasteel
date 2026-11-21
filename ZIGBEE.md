# Zigbee networks

Each Zigbee2MQTT instance has its own network: its own PAN id, extended PAN id,
network key (all `GENERATE`d once by Zigbee2MQTT) and its own channel. Channels
15, 20 and 25 stay clear of WiFi channels 1, 6 and 11, whichever of those UniFi
picks. Zigbee2MQTT runs on 10.222.15.22 (MQTT base topic `zigbee2mqttN`).

## Networks (2026-11-20)

| z2m | Coordinator | IP | Place | Channel | PAN id | Extended PAN id | Z2M image |
| --- | --- | --- | --- | --- | --- | --- | --- |
| z2m1 | `wifi-01` (XIAO ESP32-C6, the former .63) | 192.168.5.28 (Outdoor) | loods 3 | 20 | 0x4DD9 | 54:6a:15:6c:81:56:33:41 | tostmann (required with WiFi) |
| z2m2 | `wifi-02` (XIAO ESP32-C6, MAC 58:e6:c5:0f:4b:20) | 10.222.15.37 | desk (testing) | 15 | ? | ? | tostmann (required with WiFi) |
| z2m3 | `eth-01` (XIAO ESP32-C6 + W5500) | 10.222.15.29 | desk (testing; meant for loods 2) | 15 | 0xE2F6 | a9:ef:15:62:4e:54:26:5a | standard (tostmann possible) |
| z2m4 | TubesZB 2023-01 | 10.222.15.62 | house, cellar | 25 | 0x845C | 81:2c:ca:3d:99:c2:f3:b0 | standard |
| – | `uart-01` on the Olimex (`uart-host-01`) | 10.222.15.61 | desk (testing) | 20 | 0xE5A7 (test network) | – | no z2m yet |
| – | `uart-02` on the second Olimex (`uart-host-02`, the former TubesZB 2023 of loods 3) | 10.222.15.51 | desk (testing) | – | no network yet | – | no z2m yet |

z2m2 and z2m3 share channel 15 while both lie on the desk; one switch (moved
from eth-01) is on wifi-02. Before a coordinator goes to its place, give its
network a channel of its own there.

Each z2m has its own generated network key. The TubesZB 2023-02 in loods 3
(192.168.5.112) is off duty: see below.

UART coordinators: a C6 without a network (`zb-coord-uart-NN.yaml`, package
`devices/zigbee_coordinator_zboss_uart.yaml`) on the UEXT of an Olimex
ESP32-PoE (`zb-coord-uart-host-NN.yaml`, package
`devices/zigbee_coordinator_uart_host.yaml`). The host file sets `network`,
`static_ip` and `c6_firmware_url` (release `uart-NN`, published with
`tools/publish_c6_firmware.sh zb-coord-uart-NN.yaml`). A new C6 gets its first
firmware over USB (it runs Seeed's demo out of the box); after that "Update C6"
on its host. uart-host-02 has WireGuard off: it needs its own key and peer
first (uart-host-01 uses `private_key_zigbee`).

ESPHome configs: `zb-coord-wifi-01.yaml`, `zb-coord-eth-01.yaml`
(package `devices/zigbee_coordinator_zboss*.yaml`), `tubeszb-cc2652p7-poe-2023-01/-02.yaml`
(package `devices/tubeszb_cc2652p7_poe_2023.yaml`).

The component comes from the branch `esphome-signal` of
whulshof/esp-coordinator (substitution `zboss_ref`): PR #21 (merged upstream
2026-11-17), PR #22 (open) and the signal sensors (no PR yet). Those show
the Zigbee frames received in the last minute: "Zigbee RSSI" (average, dBm),
"Zigbee RSSI weakest", "Zigbee LQI" and "Zigbee frames received"; first seen
on wifi-02 on 2026-11-20 (-63 dBm, LQI 87). An in-use coordinator gets them
with its next build.

Spare: `zb-coord-uart-01.yaml` (ESP32-C6, MAC 58:e6:c5:0f:4c:30, the
former tostmann-firmware C6 of the Olimex), package
`devices/zigbee_coordinator_zboss_uart.yaml`: the NCP on UART1, on UART0's
pins (GPIO16 TX = XIAO D6, GPIO17 RX = D7, 115200), `connection_type: serial`,
no network. It sits on the UEXT of the Olimex ESP32-PoE
(`zb-coord-uart-host-01.yaml`, 10.222.15.61), which passes the UART on
to TCP: Zigbee2MQTT `port: tcp://10.222.15.61:6638`, `adapter: zboss`. No
OTA, API or web page; log over USB. Its sensors go once a minute as a line
of text over a second UART (XIAO D4, 9600 baud) to the Olimex, which shows
them as its own: Zigbee RSSI, RSSI weakest, LQI, frames received, channel,
PAN ID, network lost and the C6's firmware version.

Wiring: UEXT 3 (GPIO4, TX) to D7, UEXT 4 (GPIO36, RX) to D6, UEXT 5 (GPIO16)
to D2 (download line), UEXT 6 (GPIO13) to D4 (status line), UEXT 1/2 to
3V3/GND.

Update the C6 without USB, from the Olimex (component
`external_components/my_components/c6_flasher`, Espressif's
esp-serial-flasher; the method of the TubesZB flasher):

1. Compile `zb-coord-uart-01.yaml` (Device Builder).
2. `tools/publish_c6_firmware.sh`: puts the factory image on GitHub as asset
   of release `uart-01` (public; it refuses an image with a secret in it).
3. Olimex button "Update C6": downloads the image, pulls D2 low (the C6
   reboots into the ROM loader), writes and verifies it, and resets the C6
   with its watchdog. About 1.5 minutes; state in the sensor "C6 update".
   The bridge pauses meanwhile, Zigbee2MQTT reconnects by itself. The Zigbee
   network stays.

A failed download leaves the C6 untouched; a failure while writing leaves it
in download mode: press "Update C6" again. From a PC instead: button "C6
download mode", stop Zigbee2MQTT, then
`tools/flash_c6_over_uart.py socket://10.222.15.61:6638 firmware.factory.bin`.
If the C6 does not answer in download mode: hold BOOT (B), tap RESET (R),
release BOOT, update again.

The reboot into download mode must be a system reset
(`devices/include/c6_download_mode.h`): `esp_restart()` is a CPU reset on the
C6, which keeps the NCP's pin routing, and then the ROM does not hear GPIO17.

Tested 2026-11-20 on the Olimex: Zigbee2MQTT resumed a network through
tcp://10.222.15.61:6638; "Update C6" fetched the release asset and wrote
794320 bytes in 85 s, verified, after which Zigbee2MQTT resumed; the PC
route (41 s) as well. Not tested: an interrupted download or write.
The status line arrived on the Olimex (channel 20, PAN 0xE5A7 of the test
network, 0 frames: no devices on it yet).

## Steps

| # | What | Status |
| --- | --- | --- |
| 1 | Flash `eth-01`, z2m3 to `tcp://10.222.15.29:6638` | done (2026-11-17) |
| 2 | z2m4: new network on channel 25 (no devices attached) | done (2026-11-17) |
| 3 | z2m1: the TubesZB in loods 3 could not form a new network (below), so `wifi-01` right away: .63 flashed with `wifi-01`, into loods 3; z2m1 on `tcp://192.168.5.28:6638`, tostmann's image, new network on channel 20 | done (2026-11-17) |
| 4 | Pair the loods 3 devices to z2m1: routers first, then battery devices | open |
| 5 | z2m2's devices (e.g. the ZBMINI) re-paired to the z2m of their place; `zbtemploods2` to z2m3 | open |
| 6 | Flash the house TubesZB (2023-01) with the standard config; new API key in the kernel (the loods 3 one already runs it) | when convenient |
| 7 | Remove the DHCP reservation for 58:e6:c5:0f:49:6f (eth-01 has a fixed IP now) | open |
| 8 | TubesZB loods 3: find out why it cannot form a network (below) | open |

Open question: the 2022 tube (`tubezb-cc2652-poe-2022-202603281.yaml`, 192.168.5.61):
the old one in loods 2? Then to `Not in use`.

## Zigbee2MQTT configuration

New network (once): set the three values to `GENERATE`, rename
`coordinator_backup.json`, restart. Zigbee2MQTT then writes the real values into
`configuration.yaml`; keep them. A value that is missing (not `GENERATE`) gets
the public default (PAN 0x1A62, extended `dd:dd:…`, the well-known key): that
happened to z2m1's `ext_pan_id` once. One `advanced:` block only (a second one,
or a duplicate key, makes the file invalid and Zigbee2MQTT refuses to start).

Lessons from 2026-11-17:
- "Configuration is not consistent with adapter state/backup": the adapter
  still has the old network and `coordinator_backup.json` protects it. Rename
  the backup (inside the container: `/app/data`) and restart.
- `database.db` keeps every device ever paired; Zigbee2MQTT puts them back into
  `devices:` (named by IEEE address) even with an empty `devices: {}`. Rename
  it for a really clean start.
- `adapter:` must match the coordinator: `zstack` for the TubesZB, `zboss` for
  the ESP32-C6. A wrong one only shows at the next restart.

`transmit_power: 20` (tostmann's advice: uses the ESP32-C6's amplifier; the
CC2652P7 also goes up to 20 dBm). 20 dBm is the EU limit for 2.4 GHz (EIRP);
with an external antenna with gain it is slightly above that.

z2m1 (`wifi-01`), image `ghcr.io/tostmann/zigbee2mqtt-esp32:latest` (required
for the WiFi build: longer TCP timeouts):

```yaml
serial:
  port: tcp://192.168.5.28:6638
  adapter: zboss
advanced:
  channel: 20
  # pan_id, ext_pan_id, network_key: generated, keep them
  transmit_power: 20
```

z2m3 (`eth-01`): `tcp://10.222.15.29:6638`, `adapter: zboss`, channel 15.

z2m4 (TubesZB house): `tcp://10.222.15.62:6638`, `adapter: zstack`, channel 25,
`transmit_power: 20`.

## TubesZB loods 3 (192.168.5.112): cannot form a network

On 2026-11-17 every attempt to form a new network failed after 100 s with
"network commissioning timed out - most likely network with the same panId or
extendedPanId already exists nearby" (ZDO stateChangeInd). Same result with
generated, random and default PAN ids, on channel 15 and 20, with the original
ESPHome firmware and after the standard config, and on a freshly flashed,
erased CC2652P7. Its existing network had run fine for months; forming starts
with a scan, which it apparently no longer manages. The house TubesZB (same
model, same firmware) formed its network on channel 25 at the first try.

Not tried yet: power off (PoE unplugged, 30 s; module resets keep the CC2652
powered), Z-Stack debug log (`log_level: debug`, look at the stateChangeInd
states), and the swap test (z2m1 on the house TubesZB with z2m4 stopped).

Flashing its CC2652: the built-in "Flash CC2652" button erases the chip and then
fails (no ACK on the first SEND_DATA, at 500000 and 115200 baud). What works:
button "Trigger Zigbee Module Bootloader", then from a PC with cc2538-bsl
(github.com/JelmerT/cc2538-bsl):

    cc2538-bsl -p socket://192.168.5.112:6638 -evw CC1352P7_coordinator_20250321.hex

(the P7 firmware from tube0013/tube_gateways `misc/`; it has a CC2652P7, not a
P2), then "Zigbee Module Reset". Stop the z2m that uses it first.

## Switching coordinators

A device is in one network at a time. Moving a network to another coordinator
(same PAN ids, key and channel from `configuration.yaml`) keeps the network,
but the coordinator's IEEE address (trust center) and frame counters change:
some devices then need re-pairing. Tostmann's image backs up the whole NVRAM of
an ESP coordinator (incl. frame counters) in `coordinator_backup.json` and
restores it onto a blank C6, so a spare C6 can take over without re-pairing.
Between the TubesZB (Z-Stack) and an ESP coordinator (ZBOSS) that does not work.

## Coordinator IPs and ports

- `allowed_ips` on the ESP coordinators: only 10.222.15.22 (the Zigbee2MQTT
  host) may open port 6638.
- `eth-01`: fixed IP from `devices/network` (connection_type ethernet).
- `wifi-01`: network Outdoor, fixed 192.168.5.28; no access point (it cannot
  share the radio with Zigbee), power save and roaming scans off. In loods 3:
  WiFi -35 dBm on the external antenna.
- The TubesZB run `stream_server` without `allowed_ips`; API key, OTA password
  and web login come with the standard config.
