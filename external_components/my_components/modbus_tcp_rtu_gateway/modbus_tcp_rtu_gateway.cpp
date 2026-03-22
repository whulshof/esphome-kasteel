#include "modbus_tcp_rtu_gateway.h"

#include <cerrno>
#include <cstring>

#include <lwip/sockets.h>
#include <lwip/inet.h>
#include <unistd.h>

namespace esphome {
namespace modbus_tcp_rtu_gateway {

static const char *const TAG = "modbus_tcp_rtu_gateway";

void ModbusTcpRtuGateway::setup() {
  ESP_LOGI(TAG, "Setting up Modbus TCP RTU Gateway");
  this->set_healthy_(false);
  this->set_client_connected_(false);
}

void ModbusTcpRtuGateway::dump_config() {
  ESP_LOGCONFIG(TAG, "Modbus TCP RTU Gateway:");
  ESP_LOGCONFIG(TAG, "  TCP Port: %u", this->port_);
  ESP_LOGCONFIG(TAG, "  Response timeout: %u ms", this->response_timeout_ms_);
}

float ModbusTcpRtuGateway::get_setup_priority() const {
  return setup_priority::AFTER_WIFI;
}

void ModbusTcpRtuGateway::set_healthy_(bool v) {
  if (this->healthy_ == v)
    return;
  this->healthy_ = v;
  if (this->healthy_sensor_ != nullptr)
    this->healthy_sensor_->publish_state(v);
}

void ModbusTcpRtuGateway::set_client_connected_(bool v) {
  if (this->client_connected_ == v)
    return;
  this->client_connected_ = v;
  if (this->client_connected_sensor_ != nullptr)
    this->client_connected_sensor_->publish_state(v);
}

bool ModbusTcpRtuGateway::ensure_server_() {
  if (this->listen_sock_ >= 0)
    return true;

  this->listen_sock_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  if (this->listen_sock_ < 0) {
    ESP_LOGE(TAG, "socket() failed: errno=%d", errno);
    this->set_healthy_(false);
    return false;
  }

  int opt = 1;
  ::setsockopt(this->listen_sock_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(this->port_);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (::bind(this->listen_sock_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
    ESP_LOGE(TAG, "bind() failed: errno=%d", errno);
    ::close(this->listen_sock_);
    this->listen_sock_ = -1;
    this->set_healthy_(false);
    return false;
  }

  if (::listen(this->listen_sock_, 1) != 0) {
    ESP_LOGE(TAG, "listen() failed: errno=%d", errno);
    ::close(this->listen_sock_);
    this->listen_sock_ = -1;
    this->set_healthy_(false);
    return false;
  }

  ESP_LOGI(TAG, "Listening on TCP port %u", this->port_);
  this->set_healthy_(true);
  return true;
}

void ModbusTcpRtuGateway::accept_client_() {
  sockaddr_in src_addr{};
  socklen_t addrlen = sizeof(src_addr);

  int sock = ::accept(this->listen_sock_, reinterpret_cast<sockaddr *>(&src_addr), &addrlen);
  if (sock < 0) {
    return;
  }

  this->client_sock_ = sock;
  this->set_client_connected_(true);

  char ipbuf[32];
  inet_ntop(AF_INET, &src_addr.sin_addr, ipbuf, sizeof(ipbuf));
  ESP_LOGI(TAG, "Client connected from %s", ipbuf);
}

void ModbusTcpRtuGateway::close_client_() {
  if (this->client_sock_ >= 0) {
    ::shutdown(this->client_sock_, 0);
    ::close(this->client_sock_);
    this->client_sock_ = -1;
  }
  this->set_client_connected_(false);
}

void ModbusTcpRtuGateway::handle_client_() {
  uint8_t buf[260];

  int len = ::recv(this->client_sock_, buf, sizeof(buf), MSG_DONTWAIT);
  if (len == 0) {
    ESP_LOGI(TAG, "Client disconnected");
    this->close_client_();
    return;
  }

  if (len < 0) {
    return;
  }

  ESP_LOGD(TAG, "Received %d bytes", len);

  // Placeholder: echo terug
  int sent = ::send(this->client_sock_, buf, len, 0);
  if (sent < 0) {
    ESP_LOGW(TAG, "send() failed, closing client");
    this->close_client_();
  }
}

void ModbusTcpRtuGateway::loop() {
  if (!this->ensure_server_())
    return;

  if (this->client_sock_ < 0) {
    this->accept_client_();
    return;
  }

  this->handle_client_();
}

}  // namespace modbus_tcp_rtu_gateway
}  // namespace esphome