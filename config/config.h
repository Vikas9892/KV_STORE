#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

struct Config {
    uint16_t    port                 = 7379;
    std::size_t workers              = 0;        // 0 = hardware_concurrency
    std::size_t max_clients          = 1024;
    std::string log_level            = "INFO";
    std::string wal_path             = "kv.log";
    std::string snapshot_path        = "kv.snap";
    std::size_t snapshot_interval_s  = 300;      // 0 = disabled
    std::size_t kv_initial_capacity  = 0;        // 0 = default
    // Replication (optional — leave replica_host empty to disable)
    std::string replica_host         = "";
    uint16_t    replica_port         = 7380;

    static Config from_file(const std::string& path);
    static void   write_default(const std::string& path);
};
