#pragma once
// Which Zigbee2MQTT uses this coordinator. The TCP peer address says little:
// z2m in Docker connects from the Docker host's IP. Every z2m publishes
// <base_topic>/bridge/info (retained) with its config, serial.port included;
// the z2m whose serial.port names this coordinator is the one.
// Package: devices/zigbee_z2m_lookup.yaml.
#include <map>
#include <string>
#include <vector>
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/components/mqtt/mqtt_client.h"

// Per base_topic: does its bridge/info name this bridge, is it online
// (bridge/state). A z2m that stopped keeps its retained bridge/info, possibly
// with an old serial.port, so an online one wins.
struct Z2mSeen {
  bool ours{false};
  bool online{false};
  bool checked{false};       // its bridge/info read since it came online
};

inline std::map<std::string, Z2mSeen> &z2m_seen() {
  static std::map<std::string, Z2mSeen> seen;
  return seen;
}

// base_topic of the z2m that uses this bridge, empty if none: the online one,
// else any (several only when configs are stale).
inline std::string z2m_for_bridge() {
  std::string any;
  for (const auto &kv : z2m_seen()) {
    if (!kv.second.ours)
      continue;
    if (kv.second.online)
      return kv.first;
    if (any.empty())
      any = kv.first;
  }
  return any;
}

// bridge/info is ~28 KB, which ESP-IDF's MQTT client hands over in 1 KB
// chunks through a queue of 31: all z2m's retained messages at once (one
// '+/bridge/info' subscription) overflow it and none arrives whole. So
// '+/bridge/state' (small) finds the z2m's, and their bridge/info is read
// one at a time: subscribe, take the retained message, unsubscribe.
struct Z2mScan {
  std::vector<std::string> ports;  // e.g. "tcp://10.222.15.51:6638"
  std::string busy;          // base_topic whose bridge/info is subscribed
  uint32_t busy_since{0};
  bool got{false};
};

inline Z2mScan &z2m_scan() {
  static Z2mScan scan;
  return scan;
}

// No periodic re-read: a new serial.port takes a z2m restart, and then
// bridge/state goes online again.
static constexpr uint32_t Z2M_INFO_TIMEOUT_MS = 15000;

// The addresses z2m may have in serial.port (tcp://<address>:6638) for this
// coordinator: its LAN address and, with WireGuard, its tunnel address. Empty
// ones are skipped. Call once (on_boot).
inline void z2m_watch_bridge_info(std::initializer_list<const char *> addresses) {
  for (const char *a : addresses)
    if (a != nullptr && *a != '\0')
      z2m_scan().ports.push_back(std::string("tcp://") + a + ":6638");
  esphome::mqtt::global_mqtt_client->subscribe(
      "+/bridge/state",
      [](const std::string &topic, const std::string &payload) {
        // z2m 2.x: {"state":"online"}; older: online
        bool online = payload.find("online") != std::string::npos;
        Z2mSeen &s = z2m_seen()[topic.substr(0, topic.find('/'))];
        if (online && !s.online)
          s.checked = false;  // (re)started: its bridge/info may be new
        s.online = online;
      },
      0);
}

// Call every second or so (interval).
inline void z2m_scan_step() {
  auto *mqtt = esphome::mqtt::global_mqtt_client;
  Z2mScan &scan = z2m_scan();
  const uint32_t now = esphome::millis();
  if (!scan.busy.empty()) {
    if (!scan.got && now - scan.busy_since < Z2M_INFO_TIMEOUT_MS)
      return;
    if (!scan.got)
      ESP_LOGW("z2m", "no bridge/info from %s", scan.busy.c_str());
    mqtt->unsubscribe(scan.busy + "/bridge/info");
    z2m_seen()[scan.busy].checked = true;
    scan.busy.clear();
    return;
  }
  if (!mqtt->is_connected())
    return;
  for (auto &kv : z2m_seen()) {
    if (kv.second.checked)
      continue;
    scan.busy = kv.first;
    scan.busy_since = now;
    scan.got = false;
    std::string base = kv.first;
    mqtt->subscribe(
        base + "/bridge/info",
        [base](const std::string &, const std::string &payload) {
          Z2mScan &scan = z2m_scan();
          // The port as a JSON string value, so .5 does not match .51.
          bool ours = false;
          for (const auto &port : scan.ports)
            ours = ours || payload.find("\"" + port + "\"") != std::string::npos;
          Z2mSeen &s = z2m_seen()[base];
          if (ours != s.ours)
            ESP_LOGI("z2m", "%s %s this bridge", base.c_str(), ours ? "uses" : "no longer uses");
          s.ours = ours;
          if (scan.busy == base)
            scan.got = true;
        },
        0);
    return;
  }
}
