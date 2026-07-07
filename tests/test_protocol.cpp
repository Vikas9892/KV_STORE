#include "network/protocol.h"

#include <gtest/gtest.h>

// ── CommandParser::to_command ─────────────────────────────────────────────────

TEST(CommandParser, RecognisesAllCommands) {
    EXPECT_EQ(CommandParser::to_command("GET"),    Command::GET);
    EXPECT_EQ(CommandParser::to_command("SET"),    Command::SET);
    EXPECT_EQ(CommandParser::to_command("DELETE"), Command::DELETE);
    EXPECT_EQ(CommandParser::to_command("DEL"),    Command::DELETE);
    EXPECT_EQ(CommandParser::to_command("EXISTS"), Command::EXISTS);
    EXPECT_EQ(CommandParser::to_command("SIZE"),   Command::SIZE);
    EXPECT_EQ(CommandParser::to_command("CLEAR"),  Command::CLEAR);
    EXPECT_EQ(CommandParser::to_command("PING"),   Command::PING);
    EXPECT_EQ(CommandParser::to_command("BLAH"),   Command::UNKNOWN);
}

TEST(CommandParser, CaseInsensitive) {
    EXPECT_EQ(CommandParser::to_command("get"),    Command::GET);
    EXPECT_EQ(CommandParser::to_command("Set"),    Command::SET);
    EXPECT_EQ(CommandParser::to_command("pInG"),   Command::PING);
}

// ── CommandParser::parse ──────────────────────────────────────────────────────

TEST(CommandParser, ParsePing) {
    auto r = CommandParser::parse("PING");
    EXPECT_EQ(r.cmd, Command::PING);
    EXPECT_TRUE(r.key.empty());
}

TEST(CommandParser, ParseGet) {
    auto r = CommandParser::parse("GET mykey");
    EXPECT_EQ(r.cmd, Command::GET);
    EXPECT_EQ(r.key, "mykey");
}

TEST(CommandParser, ParseSet) {
    auto r = CommandParser::parse("SET name Vikas Tiwari");
    EXPECT_EQ(r.cmd, Command::SET);
    EXPECT_EQ(r.key,   "name");
    EXPECT_EQ(r.value, "Vikas Tiwari");
}

TEST(CommandParser, ParseSetSingleWord) {
    auto r = CommandParser::parse("SET age 20");
    EXPECT_EQ(r.key,   "age");
    EXPECT_EQ(r.value, "20");
}

TEST(CommandParser, ParseDelete) {
    auto r = CommandParser::parse("DELETE key");
    EXPECT_EQ(r.cmd, Command::DELETE);
    EXPECT_EQ(r.key, "key");
}

TEST(CommandParser, ParseEmptyLine) {
    auto r = CommandParser::parse("");
    EXPECT_EQ(r.cmd, Command::UNKNOWN);
}

TEST(CommandParser, ParseSize) {
    auto r = CommandParser::parse("SIZE");
    EXPECT_EQ(r.cmd, Command::SIZE);
}

// ── Response serialization ────────────────────────────────────────────────────

TEST(Response, OkSerializes) {
    EXPECT_EQ(Response::ok().serialize(), "+OK\r\n");
}

TEST(Response, ValueSerializes) {
    EXPECT_EQ(Response::value("hello").serialize(), "+hello\r\n");
}

TEST(Response, IntegerSerializes) {
    EXPECT_EQ(Response::integer_r(42).serialize(),  ":42\r\n");
    EXPECT_EQ(Response::integer_r(0).serialize(),   ":0\r\n");
}

TEST(Response, ErrorSerializes) {
    EXPECT_EQ(Response::error("NOT_FOUND").serialize(), "-ERR NOT_FOUND\r\n");
}
