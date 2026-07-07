#include "config/config.h"
#include "core/kv_store.h"
#include "server/epoll_server.h"
#include "server/signal_handler.h"
#include "utils/logger.h"

#include <iostream>
#include <stdexcept>

static LogLevel parse_log_level(const std::string& s) {
    if (s == "DEBUG") return LogLevel::DEBUG;
    if (s == "WARN")  return LogLevel::WARN;
    if (s == "ERROR") return LogLevel::ERROR;
    return LogLevel::INFO;
}

int main(int argc, char* argv[]) {
    std::string config_path = (argc > 1) ? argv[1] : "config.json";

    Config cfg;
    try { cfg = Config::from_file(config_path); }
    catch (const std::exception& e) {
        std::cerr << "[FATAL] " << e.what() << '\n'; return 1;
    }

    Logger::get().set_level(parse_log_level(cfg.log_level));
    SignalHandler::install();

    try {
        KVStore store(cfg.wal_path, cfg.snapshot_path, cfg.kv_initial_capacity);
        EpollServer server(cfg.port, store, cfg.workers);
        server.run();
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("[epoll_main] ") + e.what()); return 1;
    }
}
