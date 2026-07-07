#include "network/protocol.h"

#include <algorithm>
#include <cctype>
#include <sstream>

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

Command CommandParser::to_command(std::string_view token) {
    std::string up(token);
    std::transform(up.begin(), up.end(), up.begin(),
                   [](unsigned char c){ return std::toupper(c); });
    if (up == "GET")    return Command::GET;
    if (up == "SET")    return Command::SET;
    if (up == "DELETE" || up == "DEL") return Command::DELETE;
    if (up == "EXISTS") return Command::EXISTS;
    if (up == "SIZE")   return Command::SIZE;
    if (up == "CLEAR")  return Command::CLEAR;
    if (up == "PING")   return Command::PING;
    return Command::UNKNOWN;
}

Request CommandParser::parse(std::string_view line) {
    std::istringstream ss{std::string(line)};
    Request req;
    std::string token;
    if (!(ss >> token)) return req;
    req.cmd = to_command(token);
    if (ss >> token) req.key = token;
    std::string rest;
    if (std::getline(ss >> std::ws, rest)) req.value = rest;
    return req;
}
