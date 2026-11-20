#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart.h"

#include <atomic>
#include <string>

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

class StreamServerComponent;

namespace esphome {
namespace c6_flasher {

/// Flashes an ESP32-C6 on a plain UART (no EN/BOOT wires) from an http(s) URL.
/// The work runs in its own task; the main loop only starts it and publishes
/// the status.
class C6Flasher : public Component, public uart::UARTDevice {
 public:
  void set_url(const std::string &url) { this->url_ = url; }
  void set_download_pin(GPIOPin *pin) { this->download_pin_ = pin; }
  void set_stream_server(StreamServerComponent *s) { this->stream_server_ = s; }
  void set_status_sensor(text_sensor::TextSensor *s) { this->status_sensor_ = s; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  /// Download the image and flash the target. False when a job is running.
  bool update() { return this->start_(JOB_UPDATE); }
  /// Only put the target in its ROM download mode (to flash it from a PC).
  bool download_mode() { return this->start_(JOB_DOWNLOAD_MODE); }
  bool is_busy() const { return this->job_.load() != JOB_NONE; }

 protected:
  enum Job : uint8_t { JOB_NONE, JOB_UPDATE, JOB_DOWNLOAD_MODE };

  bool start_(Job job);
  static void task_trampoline_(void *arg);
  void run_update_();
  void pulse_download_line_();
  void set_status_(const char *fmt, ...) __attribute__((format(printf, 2, 3)));
  void pause_bridge_();
  void resume_bridge_();

  std::string url_;
  GPIOPin *download_pin_{nullptr};
  StreamServerComponent *stream_server_{nullptr};
  text_sensor::TextSensor *status_sensor_{nullptr};
  uart_port_t uart_num_{UART_NUM_MAX};

  std::atomic<Job> job_{JOB_NONE};
  SemaphoreHandle_t status_lock_{nullptr};
  char status_[96]{"idle"};
  std::atomic<uint32_t> status_seq_{0};
  uint32_t published_seq_{0};
};

template<typename... Ts> class UpdateAction : public Action<Ts...> {
 public:
  explicit UpdateAction(C6Flasher *parent) : parent_(parent) {}
  void play(const Ts &...x) override { this->parent_->update(); }

 protected:
  C6Flasher *parent_;
};

template<typename... Ts> class DownloadModeAction : public Action<Ts...> {
 public:
  explicit DownloadModeAction(C6Flasher *parent) : parent_(parent) {}
  void play(const Ts &...x) override { this->parent_->download_mode(); }

 protected:
  C6Flasher *parent_;
};

}  // namespace c6_flasher
}  // namespace esphome
