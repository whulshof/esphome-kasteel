#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace modbus_tcp_rtu_gateway {

class ModbusTcpRtuGateway : public Component, public uart::UARTDevice {
 public:
  void set_port(uint16_t port) { this->port_ = port; }
  void set_response_timeout_ms(uint32_t timeout_ms) { this->response_timeout_ms_ = timeout_ms; }

  void set_healthy_sensor(binary_sensor::BinarySensor *sens) { this->healthy_sensor_ = sens; }
  void set_client_connected_sensor(binary_sensor::BinarySensor *sens) { this->client_connected_sensor_ = sens; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

 protected:
  uint16_t port_{502};
  uint32_t response_timeout_ms_{500};

  int listen_sock_{-1};
  int client_sock_{-1};

  bool healthy_{false};
  bool client_connected_{false};

  binary_sensor::BinarySensor *healthy_sensor_{nullptr};
  binary_sensor::BinarySensor *client_connected_sensor_{nullptr};

  bool ensure_server_();
  void accept_client_();
  void handle_client_();
  void close_client_();

  void set_healthy_(bool v);
  void set_client_connected_(bool v);
};

}  // namespace modbus_tcp_rtu_gateway
}  // namespace esphome