#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

// RAII wrapper around a POSIX file descriptor.
// Moveable, not copyable — exactly one owner at a time.
class Socket {
public:
    static Socket listen_on(int port, int backlog = 128);

    Socket()                           = default;
    explicit Socket(int fd) : m_fd(fd) {}
    ~Socket();

    Socket(const Socket&)            = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&&) noexcept;
    Socket& operator=(Socket&&) noexcept;

    bool valid() const { return m_fd >= 0; }
    int  fd()    const { return m_fd; }

    Socket accept_client(std::string& peer_addr) const;
    void   send_all(std::string_view data) const;
    std::optional<std::string> recv_line();   // reads until \n, strips \r

private:
    int         m_fd      = -1;
    std::string m_buf;                        // partial-read accumulator
};
