#include "core/snapshot.h"
#include "utils/logger.h"

#include <chrono>
#include <cstring>
#include <fstream>
#include <mutex>
#include <stdexcept>

// ── CRC32 (Ethernet/ZIP reflected polynomial 0xEDB88320) ─────────────────────

static uint32_t    s_crc_table[256];
static std::once_flag s_crc_once;

static void build_crc_table() {
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int j = 0; j < 8; ++j)
            c = (c >> 1) ^ (0xEDB88320u & -(c & 1u));
        s_crc_table[i] = c;
    }
}

static uint32_t crc32_buf(const void* data, std::size_t len, uint32_t running = 0xFFFFFFFFu) {
    std::call_once(s_crc_once, build_crc_table);
    const auto* p = static_cast<const uint8_t*>(data);
    while (len--) running = (running >> 8) ^ s_crc_table[(running ^ *p++) & 0xFF];
    return running;
}

// ── Snapshot::save ────────────────────────────────────────────────────────────

void Snapshot::save(const std::unordered_map<std::string, std::string>& data,
                    const std::string& path) {
    std::string tmp = path + ".tmp";
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("snapshot: cannot open " + tmp);

    // Accumulate entire payload in memory so we can CRC it cleanly
    std::string payload;
    payload.reserve(22 + data.size() * 64);

    auto append_u16 = [&](uint16_t v) {
        payload += char(v & 0xFF); payload += char((v >> 8) & 0xFF);
    };
    auto append_u32 = [&](uint32_t v) {
        for (int i = 0; i < 4; ++i) payload += char((v >> (i*8)) & 0xFF);
    };
    auto append_u64 = [&](uint64_t v) {
        for (int i = 0; i < 8; ++i) payload += char((v >> (i*8)) & 0xFF);
    };
    auto append_str = [&](const std::string& s) {
        append_u32(static_cast<uint32_t>(s.size()));
        payload += s;
    };

    payload += "KVSS";
    append_u16(VERSION);
    append_u64(static_cast<uint64_t>(
        std::chrono::system_clock::now().time_since_epoch().count()));
    append_u64(static_cast<uint64_t>(data.size()));

    for (const auto& [k, v] : data) { append_str(k); append_str(v); }

    uint32_t crc = ~crc32_buf(payload.data(), payload.size());
    out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    for (int i = 0; i < 4; ++i) { char c = char((crc >> (i*8)) & 0xFF); out.write(&c, 1); }

    if (!out) throw std::runtime_error("snapshot: write error on " + tmp);
    out.close();

    if (std::rename(tmp.c_str(), path.c_str()) != 0)
        throw std::runtime_error("snapshot: rename failed");

    LOG_INFO("[snapshot] Saved " + std::to_string(data.size()) + " entries → " + path);
}

// ── Snapshot::load ────────────────────────────────────────────────────────────

std::size_t Snapshot::load(const std::string& path,
                           std::unordered_map<std::string, std::string>& data) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return 0;

    auto file_size = static_cast<std::size_t>(in.tellg());
    if (file_size < 26) { LOG_WARN("[snapshot] Too small: " + path); return 0; }
    in.seekg(0);

    std::string buf(file_size, '\0');
    if (!in.read(buf.data(), static_cast<std::streamsize>(file_size))) {
        LOG_WARN("[snapshot] Read error: " + path); return 0;
    }

    // CRC check — last 4 bytes are the checksum of everything before them
    uint32_t stored = 0;
    for (int i = 0; i < 4; ++i)
        stored |= uint32_t(uint8_t(buf[file_size - 4 + i])) << (i * 8);
    uint32_t calc = ~crc32_buf(buf.data(), file_size - 4);
    if (stored != calc) {
        LOG_WARN("[snapshot] CRC mismatch in " + path + " — skipping corrupt file");
        return 0;
    }

    if (std::memcmp(buf.data(), "KVSS", 4) != 0)
        { LOG_WARN("[snapshot] Bad magic: " + path); return 0; }

    uint16_t ver = uint16_t(uint8_t(buf[4])) | uint16_t(uint8_t(buf[5]) << 8);
    if (ver != VERSION)
        { LOG_WARN("[snapshot] Unsupported version: " + std::to_string(ver)); return 0; }

    uint64_t count = 0;
    for (int i = 0; i < 8; ++i) count |= uint64_t(uint8_t(buf[14 + i])) << (i * 8);
    data.reserve(static_cast<std::size_t>(count));

    std::size_t pos = 22;
    const std::size_t end = file_size - 4;

    auto read_u32 = [&](uint32_t& out) -> bool {
        if (pos + 4 > end) return false;
        out = 0;
        for (int i = 0; i < 4; ++i) out |= uint32_t(uint8_t(buf[pos + i])) << (i * 8);
        pos += 4; return true;
    };

    for (uint64_t i = 0; i < count; ++i) {
        uint32_t klen, vlen;
        if (!read_u32(klen) || pos + klen > end) break;
        std::string key(buf.data() + pos, klen); pos += klen;
        if (!read_u32(vlen) || pos + vlen > end) break;
        std::string val(buf.data() + pos, vlen); pos += vlen;
        data.emplace(std::move(key), std::move(val));
    }

    LOG_INFO("[snapshot] Loaded " + std::to_string(data.size()) + " entries ← " + path);
    return data.size();
}
