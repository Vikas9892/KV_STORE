#include "config/config.h"
#include "utils/logger.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>

using json = nlohmann::json;

Config Config::from_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        LOG_WARN("[config] File not found: " + path + " — using defaults");
        return {};
    }

    json j;
    try { f >> j; }
    catch (const json::exception& e) {
        throw std::runtime_error(std::string("config parse error: ") + e.what());
    }

    Config cfg;
    if (j.contains("port"))                cfg.port                = j["port"].get<uint16_t>();
    if (j.contains("workers"))             cfg.workers             = j["workers"].get<std::size_t>();
    if (j.contains("max_clients"))         cfg.max_clients         = j["max_clients"].get<std::size_t>();
    if (j.contains("log_level"))           cfg.log_level           = j["log_level"].get<std::string>();
    if (j.contains("wal_path"))            cfg.wal_path            = j["wal_path"].get<std::string>();
    if (j.contains("snapshot_path"))       cfg.snapshot_path       = j["snapshot_path"].get<std::string>();
    if (j.contains("snapshot_interval_s")) cfg.snapshot_interval_s = j["snapshot_interval_s"].get<std::size_t>();
    if (j.contains("kv_initial_capacity")) cfg.kv_initial_capacity = j["kv_initial_capacity"].get<std::size_t>();
    if (j.contains("replica_host"))        cfg.replica_host        = j["replica_host"].get<std::string>();
    if (j.contains("replica_port"))        cfg.replica_port        = j["replica_port"].get<uint16_t>();

    LOG_INFO("[config] Loaded from " + path);
    return cfg;
}

void Config::write_default(const std::string& path) {
    json j = {
        {"port",                 7379},
        {"workers",              0},
        {"max_clients",          1024},
        {"log_level",            "INFO"},
        {"wal_path",             "kv.log"},
        {"snapshot_path",        "kv.snap"},
        {"snapshot_interval_s",  300},
        {"kv_initial_capacity",  0},
        {"replica_host",         ""},
        {"replica_port",         7380}
    };
    std::ofstream f(path);
    f << j.dump(4) << '\n';
    LOG_INFO("[config] Default config written to " + path);
}
