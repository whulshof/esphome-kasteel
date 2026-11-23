#pragma once
// IP of the client on tube0013's stream_server (one client at a time), empty
// when none. client_id_ ("ip:port") is protected; a pointer to a protected
// member, taken in a derived class, may be used on any instance, so no fork
// of the component is needed. The pump task writes client_id_ before it sets
// connected_ and rewrites it only on the next accept (after a disconnect),
// so reading it while is_connected() is safe.
#include <string>
#include "esphome/components/stream_server/stream_server.h"

struct StreamServerClientPeek : StreamServerComponent {
  static std::string ip(StreamServerComponent *s) {
    if (!s->is_connected())
      return "";
    auto id = &StreamServerClientPeek::client_id_;
    std::string peer(s->*id);
    size_t colon = peer.rfind(':');
    if (colon != std::string::npos)
      peer.erase(colon);
    return peer;
  }
};

inline std::string stream_server_client(StreamServerComponent *s) { return StreamServerClientPeek::ip(s); }
