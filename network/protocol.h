#pragma once

#include <cstdint>
#include <string>
#include <string_view>

enum class Command { GET, SET, DELETE, EXISTS, SIZE, CLEAR, PING, SETEX, TTL, STATS, UNKNOWN };

struct Request {
    Command     cmd         = Command::UNKNOWN;
    std::string key;
    std::string value;
    int64_t     ttl_seconds = 0;
};

struct Response {
    enum class Type { OK, VALUE, INTEGER, ERROR };
    Type        type    = Type::ERROR;
    std::string payload;
    int64_t     integer = 0;

    static Response ok();
    static Response value(std::string v);
    static Response integer_r(int64_t n);
    static Response error(std::string msg);

    std::string serialize() const;
};

class CommandParser {
public:
    static Request parse(std::string_view line);
    static Command to_command(std::string_view token);
};
