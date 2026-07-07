#pragma once

#include "core/kv_store.h"
#include "network/protocol.h"
#include "network/socket.h"
#include "server/replicator.h"

#include <string>

class ClientSession {
public:
    ClientSession(Socket sock, std::string peer, KVStore& store,
                  Replicator* replicator = nullptr);
    ~ClientSession();

    ClientSession(const ClientSession&)            = delete;
    ClientSession& operator=(const ClientSession&) = delete;
    ClientSession(ClientSession&&)                 = default;

    void handle();
    const std::string& peer() const { return m_peer; }

private:
    Socket      m_sock;
    std::string m_peer;
    KVStore&    m_store;
    Replicator* m_replicator;   // nullable

    Response dispatch(const Request& req);
};
