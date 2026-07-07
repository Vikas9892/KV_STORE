#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

// Binary snapshot — format v1
//
//   [4]  magic     "KVSS"
//   [2]  version   0x0001  (little-endian)
//   [8]  timestamp system_clock nanoseconds (little-endian)
//   [8]  count     entry count (little-endian)
//   for each entry:
//     [4] key_len  (little-endian)
//     [N] key bytes
//     [4] val_len  (little-endian)
//     [M] val bytes
//   [4]  crc32     CRC32 of all preceding bytes (little-endian)
//
// Write is atomic: data goes to path+".tmp" then renamed over path.
class Snapshot {
public:
    static constexpr uint16_t VERSION = 1;

    static void save(const std::unordered_map<std::string, std::string>& data,
                     const std::string& path);

    // Returns number of entries loaded, 0 if file absent or corrupt.
    static std::size_t load(const std::string& path,
                            std::unordered_map<std::string, std::string>& data);
};
