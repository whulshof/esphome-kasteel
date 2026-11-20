#!/usr/bin/env python3
"""Flash an ESP32-C6 through a plain UART link (no EN/BOOT wires), e.g. the
Olimex bridge of zigbee-coordinator-uart-01:

  1. Put the C6 in download mode: button "C6 download mode" on the Olimex
     (download line low for 3 s; the firmware reboots into the ROM loader).
  2. Stop Zigbee2MQTT (the bridge takes one client).
  3. tools/flash_c6_over_uart.py socket://10.222.15.61:6638 firmware.factory.bin
  4. Start Zigbee2MQTT again.

The NCP's Zigbee network (zb_storage) is outside the factory image and stays.

esptool cannot leave download mode over such a link: RTS does nothing over
TCP and esptool 4.x refuses its watchdog reset on the C6 (a USB-Serial/JTAG
bug that does not apply to UART). So after writing, this script clears
LP_AON_FORCE_DOWNLOAD_BOOT and resets the chip with the LP watchdog itself.

Needs esptool 4.x (pip install esptool). Without an image it only resets.
"""
import argparse
import time

import esptool

LP_AON_SYS_CFG_REG = 0x600B1000 + 0x34
LP_AON_FORCE_DOWNLOAD_BOOT = 1 << 30
LP_WDT_BASE = 0x600B1C00
LP_WDT_CONFIG0 = LP_WDT_BASE + 0x00
LP_WDT_CONFIG1 = LP_WDT_BASE + 0x04
LP_WDT_WPROTECT = LP_WDT_BASE + 0x18
LP_WDT_WKEY = 0x50D83AA1


def connect(port, baud, tries=10):
    # a bridge may need a moment to accept the next client
    for i in range(tries):
        try:
            return esptool.cmds.detect_chip(port, baud, "no_reset", False, 3)
        except esptool.FatalError:
            if i == tries - 1:
                raise
            time.sleep(1)


def watchdog_reset(port, baud):
    esp = connect(port, baud)
    if esp.CHIP_NAME != "ESP32-C6":
        raise SystemExit(f"expected an ESP32-C6, found {esp.CHIP_NAME}")
    esp.write_reg(LP_AON_SYS_CFG_REG, 0, LP_AON_FORCE_DOWNLOAD_BOOT)
    esp.write_reg(LP_WDT_WPROTECT, LP_WDT_WKEY)
    esp.write_reg(LP_WDT_CONFIG1, 2000)
    # enable, stage 0 = system reset, flash boot mode, reset pulse length
    esp.write_reg(LP_WDT_CONFIG0, (1 << 31) | (5 << 28) | (1 << 8) | 2)
    esp.write_reg(LP_WDT_WPROTECT, 0)
    time.sleep(0.5)
    print("C6 reset into its firmware")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("port", help="socket://<ip>:<port> or a serial device")
    ap.add_argument("image", nargs="?", help="firmware.factory.bin (written at 0x0)")
    ap.add_argument("--baud", type=int, default=115200,
                    help="the bridge's fixed UART speed (default 115200)")
    a = ap.parse_args()
    if a.image:
        esptool.main(["--chip", "esp32c6", "--port", a.port, "--baud", str(a.baud),
                      "--before", "no_reset", "--after", "no_reset",
                      "--connect-attempts", "10", "write_flash", "0x0", a.image])
        time.sleep(1)
    watchdog_reset(a.port, a.baud)


if __name__ == "__main__":
    main()
