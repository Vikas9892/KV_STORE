#include "network/protocol.h"

#include <cctype>

Response Response::ok()                { return {Type::OK, "OK", 0}; }
Response Response::value(std::string v){ return {Type::VALUE, std::move(v), 0}; }
Response Response::integer_r(int64_t n){ return {Type::INTEGER, {}, n}; }
Response Response::error(std::string m){ return {Type::ERROR, std::move(m), 0}; }

std::string Response::serialize() const {
    switch (type) {
        case Type::OK:
        case Type::VALUE:   return "+" + payload + "\r\n";
        case Type::INTEGER: return ":" + std::to_string(integer) + "\r\n";
        case Type::ERROR:   return "-ERR " + payload + "\r\n";
    }
    return "-ERR internal\r\n";
}

// Zero-copy case-insensitive comparison — avoids heap allocation for each token
static bool sv_ieq(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (std::toupper((unsigned char)a[i]) != std::toupper((unsigned char)b[i]))
            return false;
    return true;
}

Command CommandParser::to_command(std::string_view token) {
    if (sv_ieq(token, "GET"))    return Command::GET;
    if (sv_ieq(token, "SET"))    return Command::SET;
    if (sv_ieq(token, "DELETE") || sv_ieq(token, "DEL")) return Command::DELETE;
    if (sv_ieq(token, "EXISTS")) return Command::EXISTS;
    if (sv_ieq(token, "SIZE"))   return Command::SIZE;
    if (sv_ieq(token, "CLEAR"))  return Command::CLEAR;
    if (sv_ieq(token, "PING"))   return Command::PING;
    return Command::UNKNOWN;
}

Request CommandParser::parse(std::string_view line) {
    // Strip trailing \r if present (telnet / Windows clients)
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);

    Request     req;
    std::size_t i = 0;

    while (i < line.size() && line[i] == ' ') ++i;

    std::size_t start = i;
    while (i < line.size() && line[i] != ' ') ++i;
    if (start == i) return req;

    req.cmd = to_command(line.substr(start, i - start));

    while (i < line.size() && line[i] == ' ') ++i;
    start = i;
    while (i < line.size() && line[i] != ' ') ++i;
    req.key = std::string(line.substr(start, i - start));

    while (i < line.size() && line[i] == ' ') ++i;
    req.value = std::string(line.substr(i));

    return req;
}
