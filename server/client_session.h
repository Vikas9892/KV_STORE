#pragma once

#include "core/kv_store.h"
#include "network/protocol.h"
#include "network/socket.h"

#include <string>

class ClientSession {
public:
    ClientSession(Socket sock, std::string peer, KVStore& store);
    ~ClientSession() = default;

    ClientSession(const ClientSession&)            = delete;
    ClientSession& operator=(const ClientSession&) = delete;
    ClientSession(ClientSession&&)                 = default;

    void handle();
    const std::string& peer() const { return m_peer; }

private:
    Socket      m_sock;
    std::string m_peer;
    KVStore&    m_store;

    Response dispatch(const Request& req);
};
