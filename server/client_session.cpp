#include "server/client_session.h"
#include "utils/logger.h"
#include "utils/metrics.h"

ClientSession::ClientSession(Socket sock, std::string peer, KVStore& store)
    : m_sock(std::move(sock)), m_peer(std::move(peer)), m_store(store) {
    METRICS_INC(connected_clients);
}

ClientSession::~ClientSession() {
    Metrics::get().connected_clients.fetch_sub(1, std::memory_order_relaxed);
}

void ClientSession::handle() {
    LOG_DEBUG("[session] start " + m_peer);

    while (true) {
        auto line = m_sock.recv_line();
        if (!line) break;
        if (line->empty()) continue;

        METRICS_ADD(bytes_recv, line->size() + 2);
        METRICS_INC(total_requests);

        LOG_DEBUG("[session] recv [" + m_peer + "] " + *line);

        Request  req  = CommandParser::parse(*line);
        Response resp = dispatch(req);

        std::string wire = resp.serialize();
        METRICS_ADD(bytes_sent, wire.size());

        LOG_DEBUG("[session] send [" + m_peer + "] " + wire);
        m_sock.send_all(wire);
    }

    LOG_DEBUG("[session] close " + m_peer);
}

Response ClientSession::dispatch(const Request& req) {
    switch (req.cmd) {
        case Command::PING:
            METRICS_INC(cmd_ping);
            return Response::value("PONG");

        case Command::SET:
            if (req.key.empty()) { METRICS_INC(cmd_errors); return Response::error("usage: SET key value"); }
            m_store.set(req.key, req.value);
            METRICS_INC(cmd_set);
            return Response::ok();

        case Command::GET: {
            if (req.key.empty()) { METRICS_INC(cmd_errors); return Response::error("usage: GET key"); }
            METRICS_INC(cmd_get);
            auto val = m_store.get(req.key);
            return val ? Response::value(*val) : Response::error("NOT_FOUND");
        }

        case Command::DELETE:
            if (req.key.empty()) { METRICS_INC(cmd_errors); return Response::error("usage: DELETE key"); }
            METRICS_INC(cmd_delete);
            if (m_store.del(req.key)) return Response::ok();
            return Response::error("NOT_FOUND");

        case Command::EXISTS:
            if (req.key.empty()) { METRICS_INC(cmd_errors); return Response::error("usage: EXISTS key"); }
            METRICS_INC(cmd_exists);
            return Response::integer_r(m_store.exists(req.key) ? 1 : 0);

        case Command::SIZE:
            METRICS_INC(cmd_size);
            return Response::integer_r(static_cast<int64_t>(m_store.size()));

        case Command::CLEAR:
            METRICS_INC(cmd_clear);
            m_store.clear();
            return Response::ok();

        case Command::STATS:
            return Response::value(Metrics::get().to_string());

        case Command::SETEX:
            if (req.key.empty() || req.ttl_seconds <= 0)
                return Response::error("usage: SETEX key seconds value");
            m_store.setex(req.key, req.value, req.ttl_seconds);
            METRICS_INC(cmd_set);
            return Response::ok();

        case Command::TTL: {
            if (req.key.empty()) return Response::error("usage: TTL key");
            return Response::integer_r(m_store.ttl(req.key));
        }

        case Command::UNKNOWN:
            METRICS_INC(cmd_errors);
            return Response::error("unknown command");
    }
    return Response::error("internal error");
}
