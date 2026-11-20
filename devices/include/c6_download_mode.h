#pragma once
// ESP32-C6: reboot into the ROM's UART download mode without EN or BOOT wires
// (zigbee_coordinator_zboss_uart.yaml). A software *system* reset, not
// esp_restart(): that is a CPU reset on the C6, which keeps the pin routing of
// the NCP's UART1 on GPIO16/17, so the ROM's UART0 hears nothing on GPIO17.
// The system reset puts the peripherals and IO_MUX back to their defaults;
// LP_AON_FORCE_DOWNLOAD_BOOT lives in the LP domain and survives it.
#include "esp_rom_sys.h"
#include "soc/lp_aon_reg.h"
#include "esphome/core/application.h"

inline void c6_reboot_into_download_mode() {
  esphome::App.safe_shutdown(1000);
  REG_SET_BIT(LP_AON_SYS_CFG_REG, LP_AON_FORCE_DOWNLOAD_BOOT);
  esp_rom_software_reset_system();
}
