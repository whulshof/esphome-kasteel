#include "c6_flasher.h"

#include "esphome/core/log.h"
#include "esphome/components/uart/uart_component_esp_idf.h"
#ifdef USE_C6_FLASHER_STREAM_SERVER
#include "esphome/components/stream_server/stream_server.h"
#endif

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
// esp-serial-flasher's md5_ctx.h defines a struct MD5Context of its own, which
// clashes with the ROM's (esp_rom/md5_hash.h, pulled in by ESPHome). Only its
// size matters here (it sits inside esp_loader_flash_cfg_t), so rename it for
// this file.
#define MD5Context esp_loader_MD5Context
#include "esp_loader.h"
#include "esp_loader_io.h"
#undef MD5Context
#include "esp_timer.h"

namespace esphome {
namespace c6_flasher {

static const char *const TAG = "c6_flasher";

// The target's firmware reboots into the ROM download mode when the line is
// low for 2 s (delayed_on) and then takes up to 1 s to shut down.
static const uint32_t DOWNLOAD_LINE_LOW_MS = 3000;
static const uint32_t DOWNLOAD_BOOT_WAIT_MS = 1500;
static const uint32_t BLOCK_SIZE = 1024;  // the ROM loader's flash block
static const int64_t MAX_IMAGE_SIZE = 4 * 1024 * 1024;
static const uint8_t ESP_IMAGE_MAGIC = 0xE9;

// ESP32-C6 registers (the same ones tools/flash_c6_over_uart.py uses).
static const uint32_t LP_AON_SYS_CFG_REG = 0x600B1000 + 0x34;
static const uint32_t LP_AON_FORCE_DOWNLOAD_BOOT = 1UL << 30;
static const uint32_t LP_WDT_CONFIG0_REG = 0x600B1C00;
static const uint32_t LP_WDT_CONFIG1_REG = 0x600B1C04;
static const uint32_t LP_WDT_WPROTECT_REG = 0x600B1C18;
static const uint32_t LP_WDT_WKEY = 0x50D83AA1;

// ---- esp-serial-flasher port on the UART driver ESPHome already installed ----

struct Port {
  esp_loader_port_t base;  // first member: a Port* is its esp_loader_port_t*
  uart_port_t uart;
  int64_t time_end;
};

static Port *self(esp_loader_port_t *port) { return reinterpret_cast<Port *>(port); }

static esp_loader_error_t port_write(esp_loader_port_t *port, const uint8_t *data, uint16_t size, uint32_t timeout) {
  Port *p = self(port);
  if (uart_write_bytes(p->uart, data, size) < 0)
    return ESP_LOADER_ERROR_FAIL;
  esp_err_t err = uart_wait_tx_done(p->uart, pdMS_TO_TICKS(timeout));
  if (err == ESP_OK)
    return ESP_LOADER_SUCCESS;
  return err == ESP_ERR_TIMEOUT ? ESP_LOADER_ERROR_TIMEOUT : ESP_LOADER_ERROR_FAIL;
}

static esp_loader_error_t port_read(esp_loader_port_t *port, uint8_t *data, uint16_t size, uint32_t timeout) {
  int n = uart_read_bytes(self(port)->uart, data, size, pdMS_TO_TICKS(timeout));
  if (n < 0)
    return ESP_LOADER_ERROR_FAIL;
  return n < size ? ESP_LOADER_ERROR_TIMEOUT : ESP_LOADER_SUCCESS;
}

static void port_delay_ms(esp_loader_port_t *, uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

static void port_start_timer(esp_loader_port_t *port, uint32_t ms) {
  self(port)->time_end = esp_timer_get_time() + static_cast<int64_t>(ms) * 1000;
}

static uint32_t port_remaining_time(esp_loader_port_t *port) {
  int64_t remaining = (self(port)->time_end - esp_timer_get_time()) / 1000;
  return remaining > 0 ? static_cast<uint32_t>(remaining) : 0;
}

// No EN/BOOT wires: the download line does the first, the LP watchdog the second.
static void port_noop(esp_loader_port_t *) {}

static void port_log(esp_loader_port_t *, esp_loader_log_level_t, const char *fmt, va_list args) {
  char line[160];
  vsnprintf(line, sizeof(line), fmt, args);
  ESP_LOGD(TAG, "loader: %s", line);
}

static esp_loader_port_ops_t make_ops() {
  esp_loader_port_ops_t ops = {};
  ops.enter_bootloader = port_noop;
  ops.reset_target = port_noop;
  ops.start_timer = port_start_timer;
  ops.remaining_time = port_remaining_time;
  ops.delay_ms = port_delay_ms;
  ops.log = port_log;
  ops.write = port_write;
  ops.read = port_read;
  return ops;
}
static const esp_loader_port_ops_t PORT_OPS = make_ops();

// ---- component ----

void C6Flasher::setup() {
  this->status_lock_ = xSemaphoreCreateMutex();
  this->download_pin_->setup();
  this->download_pin_->digital_write(false);
  this->uart_num_ =
      static_cast<uart_port_t>(static_cast<uart::IDFUARTComponent *>(this->parent_)->get_hw_serial_number());
}

void C6Flasher::dump_config() {
  ESP_LOGCONFIG(TAG, "C6 flasher:");
  ESP_LOGCONFIG(TAG, "  URL: %s", this->url_.c_str());
  LOG_PIN("  Download line: ", this->download_pin_);
  ESP_LOGCONFIG(TAG, "  UART: %d", static_cast<int>(this->uart_num_));
}

void C6Flasher::loop() {
  uint32_t seq = this->status_seq_.load();
  if (seq == this->published_seq_)
    return;
  this->published_seq_ = seq;
  char copy[sizeof(this->status_)];
  xSemaphoreTake(this->status_lock_, portMAX_DELAY);
  memcpy(copy, this->status_, sizeof(copy));
  xSemaphoreGive(this->status_lock_);
  if (this->status_sensor_ != nullptr)
    this->status_sensor_->publish_state(copy);
}

void C6Flasher::set_status_(const char *fmt, ...) {
  char line[sizeof(this->status_)];
  va_list args;
  va_start(args, fmt);
  vsnprintf(line, sizeof(line), fmt, args);
  va_end(args);
  ESP_LOGI(TAG, "%s", line);
  xSemaphoreTake(this->status_lock_, portMAX_DELAY);
  memcpy(this->status_, line, sizeof(line));
  xSemaphoreGive(this->status_lock_);
  this->status_seq_++;
}

bool C6Flasher::start_(Job job) {
  Job expected = JOB_NONE;
  if (!this->job_.compare_exchange_strong(expected, job)) {
    ESP_LOGW(TAG, "Busy, request ignored");
    return false;
  }
  // 12 kB: the TLS handshake of the download runs on this task's stack.
  if (xTaskCreate(task_trampoline_, "c6_flasher", 12288, this, 5, nullptr) != pdPASS) {
    this->job_ = JOB_NONE;
    ESP_LOGE(TAG, "Could not start the task");
    return false;
  }
  return true;
}

void C6Flasher::task_trampoline_(void *arg) {
  auto *self = static_cast<C6Flasher *>(arg);
  if (self->job_.load() == JOB_UPDATE) {
    self->run_update_();
  } else {
    self->pulse_download_line_();
    self->set_status_("download mode requested");
  }
  self->job_ = JOB_NONE;
  vTaskDelete(nullptr);
}

void C6Flasher::pulse_download_line_() {
  this->download_pin_->digital_write(true);
  vTaskDelay(pdMS_TO_TICKS(DOWNLOAD_LINE_LOW_MS));
  this->download_pin_->digital_write(false);
  vTaskDelay(pdMS_TO_TICKS(DOWNLOAD_BOOT_WAIT_MS));
}

void C6Flasher::pause_bridge_() {
#ifdef USE_C6_FLASHER_STREAM_SERVER
  if (this->stream_server_ != nullptr)
    this->stream_server_->pause();
#endif
}

void C6Flasher::resume_bridge_() {
  uart_flush_input(this->uart_num_);
#ifdef USE_C6_FLASHER_STREAM_SERVER
  if (this->stream_server_ != nullptr)
    this->stream_server_->resume();
#endif
}

/// Open the URL, following redirects (a GitHub release asset redirects).
static esp_http_client_handle_t http_open(const std::string &url, int64_t *length) {
  esp_http_client_config_t cfg = {};
  cfg.url = url.c_str();
  cfg.timeout_ms = 15000;
  cfg.crt_bundle_attach = esp_crt_bundle_attach;
  cfg.buffer_size = 4096;  // room for the long Location header of a redirect
  cfg.buffer_size_tx = 2048;
  esp_http_client_handle_t client = esp_http_client_init(&cfg);
  if (client == nullptr)
    return nullptr;
  for (int hop = 0; hop < 5; hop++) {
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "HTTP open failed: %s", esp_err_to_name(err));
      break;
    }
    *length = esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    if (status == 200)
      return client;
    if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
      esp_http_client_set_redirection(client);
      esp_http_client_close(client);
      continue;
    }
    ESP_LOGE(TAG, "HTTP status %d", status);
    break;
  }
  esp_http_client_cleanup(client);
  return nullptr;
}

/// Read until the buffer is full or the body ends. Returns the bytes read, -1 on error.
static int http_read_block(esp_http_client_handle_t client, uint8_t *buf, int size) {
  int got = 0;
  while (got < size) {
    int n = esp_http_client_read(client, reinterpret_cast<char *>(buf + got), size - got);
    if (n < 0)
      return -1;
    if (n == 0)
      break;
    got += n;
  }
  return got;
}

static bool reset_into_firmware(esp_loader_t *loader) {
  uint32_t cfg = 0;
  if (esp_loader_read_register(loader, LP_AON_SYS_CFG_REG, &cfg) != ESP_LOADER_SUCCESS)
    return false;
  // Clear the download flag, then let the LP watchdog reset the whole chip.
  return esp_loader_write_register(loader, LP_AON_SYS_CFG_REG, cfg & ~LP_AON_FORCE_DOWNLOAD_BOOT) ==
             ESP_LOADER_SUCCESS &&
         esp_loader_write_register(loader, LP_WDT_WPROTECT_REG, LP_WDT_WKEY) == ESP_LOADER_SUCCESS &&
         esp_loader_write_register(loader, LP_WDT_CONFIG1_REG, 2000) == ESP_LOADER_SUCCESS &&
         esp_loader_write_register(loader, LP_WDT_CONFIG0_REG, (1UL << 31) | (5UL << 28) | (1UL << 8) | 2) ==
             ESP_LOADER_SUCCESS;
}

void C6Flasher::run_update_() {
  static uint8_t block[BLOCK_SIZE];

  // 1. The image first: nothing happens to the target while it is unreachable.
  this->set_status_("downloading");
  int64_t length = 0;
  esp_http_client_handle_t http = http_open(this->url_, &length);
  if (http == nullptr) {
    this->set_status_("failed: could not open the URL (target untouched)");
    return;
  }
  int got = (length > 0 && length <= MAX_IMAGE_SIZE) ? http_read_block(http, block, BLOCK_SIZE) : -1;
  if (got <= 0 || block[0] != ESP_IMAGE_MAGIC) {
    this->set_status_("failed: not a firmware image, %lld bytes (target untouched)", static_cast<long long>(length));
    esp_http_client_cleanup(http);
    return;
  }
  const uint32_t image_size = (static_cast<uint32_t>(length) + 3) & ~3UL;

  // 2. Take the UART from the bridge and put the target in download mode.
  this->pause_bridge_();
  this->set_status_("entering download mode");
  this->pulse_download_line_();
  uart_flush_input(this->uart_num_);

  Port port = {};
  port.base.ops = &PORT_OPS;
  port.uart = this->uart_num_;
  esp_loader_t loader = {};
  esp_loader_connect_args_t connect = ESP_LOADER_CONNECT_DEFAULT();
  connect.trials = 20;
  esp_loader_error_t err = esp_loader_init_serial(&loader, &port.base);
  if (err == ESP_LOADER_SUCCESS)
    err = esp_loader_connect(&loader, &connect);
  if (err != ESP_LOADER_SUCCESS) {
    this->set_status_("failed: no answer from the ROM loader (%d)", static_cast<int>(err));
    esp_http_client_cleanup(http);
    this->resume_bridge_();
    return;
  }
  if (esp_loader_get_target(&loader) != ESP32C6_CHIP) {
    this->set_status_("failed: the target is not an ESP32-C6 (nothing written)");
    esp_http_client_cleanup(http);
    this->resume_bridge_();
    return;
  }

  // 3. Write. From here on a failure leaves the target in download mode, so
  //    another update can finish the job.
  this->set_status_("erasing");
  esp_loader_flash_cfg_t cfg = {};
  cfg.offset = 0;
  cfg.image_size = image_size;
  cfg.block_size = BLOCK_SIZE;
  err = esp_loader_flash_start(&loader, &cfg);
  uint32_t written = 0;
  int last_decile = -1;
  while (err == ESP_LOADER_SUCCESS && got > 0) {
    uint32_t size = (static_cast<uint32_t>(got) + 3) & ~3UL;
    memset(block + got, 0xFF, size - got);
    err = esp_loader_flash_write(&loader, &cfg, block, size);
    written += got;
    int decile = static_cast<int>(static_cast<uint64_t>(written) * 10 / static_cast<uint64_t>(length));
    if (decile != last_decile) {
      last_decile = decile;
      this->set_status_("writing %d%%", decile * 10);
    }
    if (written >= static_cast<uint32_t>(length))
      break;
    got = http_read_block(http, block, BLOCK_SIZE);
  }
  esp_http_client_cleanup(http);
  if (err == ESP_LOADER_SUCCESS && written != static_cast<uint32_t>(length)) {
    this->set_status_("failed: download stopped at %u of %lld bytes; target in download mode, update again",
                      static_cast<unsigned>(written), static_cast<long long>(length));
    this->resume_bridge_();
    return;
  }
  if (err == ESP_LOADER_SUCCESS)
    err = esp_loader_flash_finish(&loader, &cfg);  // compares the MD5
  if (err != ESP_LOADER_SUCCESS) {
    this->set_status_("failed: flash error %d; target in download mode, update again", static_cast<int>(err));
    this->resume_bridge_();
    return;
  }

  // 4. Back into the new firmware.
  bool reset = reset_into_firmware(&loader);
  vTaskDelay(pdMS_TO_TICKS(1500));
  this->resume_bridge_();
  if (reset) {
    this->set_status_("done: %u bytes written and verified", static_cast<unsigned>(written));
  } else {
    this->set_status_("written and verified, but the reset failed: power-cycle the target");
  }
}

}  // namespace c6_flasher
}  // namespace esphome
