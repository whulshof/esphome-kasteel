#pragma once
// Who is on the stream_server's port (tube0013's component, one client at a
// time). With Docker that is the Docker host's IP; z2m_lookup.h finds the z2m.
#include <string>
#include "esphome/components/stream_server/stream_server.h"

// client_id_ ("ip:port") is protected; a pointer to a protected member, taken
// in a derived class, may be used on any instance, so no fork of the
// component is needed. The pump task writes client_id_ before it sets
// connected_ and rewrites it only on the next accept (after a disconnect),
// so reading it while is_connected() is safe.
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
