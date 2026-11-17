# Zigbee networks

Each Zigbee2MQTT instance has its own network: its own PAN id, extended PAN id,
network key (all `GENERATE`d once by Zigbee2MQTT) and its own channel. Channels
15, 20 and 25 stay clear of WiFi channels 1, 6 and 11, whichever of those UniFi
picks. Zigbee2MQTT runs on 10.222.15.22 (MQTT base topic `zigbee2mqttN`).

## Target

| z2m | Coordinator | IP | Place | Channel | Z2M image |
| --- | --- | --- | --- | --- | --- |
| z2m1 | first TubesZB 2023-02, then `wifi-01` (the C6 that is now .63) | 192.168.5.112, then 192.168.5.28 | loods 3 | 20 | tostmann (required with `wifi-01`) |
| z2m3 | `eth-01` (XIAO ESP32-C6 + W5500) | 10.222.15.29 | loods 2 | 15 | standard (tostmann possible) |
| z2m4 | TubesZB 2023-01 | 10.222.15.62 | house | 25 | standard |
| z2m2 | – | – | – | – | stops |

ESPHome configs: `zigbee-coordinator-wifi-01.yaml`, `zigbee-coordinator-eth-01.yaml`
(package `devices/zigbee_coordinator_zboss*.yaml`, component from
esp-coordinator PR #21), `tubeszb-cc2652p7-poe-2023-01/-02.yaml`
(package `devices/tubeszb_cc2652p7_poe_2023.yaml`).

## Steps

| # | What | Status |
| --- | --- | --- |
| 1 | Flash `eth-01`, z2m3 to `tcp://10.222.15.29:6638` | done (2026-11-17) |
| 2 | z2m1 forms the new network on the TubesZB (below), then pair the loods 3 devices: routers first, then battery devices | open |
| 3 | z2m4: new network on channel 25 (no devices attached) | open |
| 4 | Loods 3 to `wifi-01`: stop z2m2, flash .63 with `wifi-01`, C6 into loods 3, TubesZB off; z2m1 to `wifi-01` (below), image to tostmann's; re-pair devices that do not come back | later |
| 5 | z2m2's devices (e.g. the ZBMINI) re-paired to the z2m of their place | with step 4 |
| 6 | Flash both TubesZB with the standard config; new API key in the kernel | when convenient |
| 7 | Remove the DHCP reservation for 58:e6:c5:0f:49:6f (eth-01 has a fixed IP now) | open |

Open question: the 2022 tube (`tubezb-cc2652-poe-2022-202603281.yaml`, 192.168.5.61):
the old one in loods 2? Then to `Not in use`.

## Zigbee2MQTT configuration

New network (once): set the three values to `GENERATE`, rename
`coordinator_backup.json`, restart. Zigbee2MQTT then writes the real values into
`configuration.yaml`; keep them. Keep the `devices:` block: re-paired devices get
their old names back (by IEEE address). One `advanced:` block only (a second one,
or a duplicate key, makes the file invalid and Zigbee2MQTT refuses to start).

`transmit_power: 20` (tostmann's advice: uses the ESP32-C6's amplifier; the
CC2652P7 also goes up to 20 dBm). 20 dBm is the EU limit for 2.4 GHz (EIRP);
with an external antenna with gain it is slightly above that.

z2m1, step 2 (TubesZB):

```yaml
serial:
  port: tcp://192.168.5.112:6638
  adapter: zstack
advanced:
  log_level: info
  channel: 20
  pan_id: GENERATE
  ext_pan_id: GENERATE
  network_key: GENERATE
  transmit_power: 20
```

z2m1, step 4 (`wifi-01`): only `serial` changes, `advanced` stays as written
by step 2, so `wifi-01` gets the same network. The WiFi build needs tostmann's
image (`ghcr.io/tostmann/zigbee2mqtt-esp32:latest`, longer TCP timeouts).

```yaml
serial:
  port: tcp://192.168.5.28:6638
  adapter: zboss
```

z2m3 (`eth-01`, already done): port `tcp://10.222.15.29:6638`, `adapter: zboss`,
channel 15, own keys.

z2m4, step 3:

```yaml
advanced:
  channel: 25
  pan_id: GENERATE
  ext_pan_id: GENERATE
  network_key: GENERATE
  transmit_power: 20
```

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
  share the radio with Zigbee), power save and roaming scans off.
