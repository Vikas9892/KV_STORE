#include "server/client_session.h"
#include "utils/logger.h"

ClientSession::ClientSession(Socket sock, std::string peer, KVStore& store)
    : m_sock(std::move(sock)), m_peer(std::move(peer)), m_store(store) {}

void ClientSession::handle() {
    LOG_DEBUG("[session] start " + m_peer);

    while (true) {
        auto line = m_sock.recv_line();
        if (!line) break;
        if (line->empty()) continue;

        LOG_DEBUG("[session] recv [" + m_peer + "] " + *line);

        Request  req  = CommandParser::parse(*line);
        Response resp = dispatch(req);

        std::string wire = resp.serialize();
        LOG_DEBUG("[session] send [" + m_peer + "] " + wire);
        m_sock.send_all(wire);
    }

    LOG_DEBUG("[session] close " + m_peer);
}

Response ClientSession::dispatch(const Request& req) {
    switch (req.cmd) {
        case Command::PING:
            return Response::value("PONG");
        case Command::SET:
            if (req.key.empty()) return Response::error("usage: SET key value");
            m_store.set(req.key, req.value);
            return Response::ok();
        case Command::GET: {
            if (req.key.empty()) return Response::error("usage: GET key");
            auto val = m_store.get(req.key);
            return val ? Response::value(*val) : Response::error("NOT_FOUND");
        }
        case Command::DELETE:
            if (req.key.empty()) return Response::error("usage: DELETE key");
            if (m_store.del(req.key)) return Response::ok();
            return Response::error("NOT_FOUND");
        case Command::EXISTS:
            if (req.key.empty()) return Response::error("usage: EXISTS key");
            return Response::integer_r(m_store.exists(req.key) ? 1 : 0);
        case Command::SIZE:
            return Response::integer_r(static_cast<int64_t>(m_store.size()));
        case Command::CLEAR:
            m_store.clear();
            return Response::ok();
        default:
            return Response::error("unknown command");
    }
}
