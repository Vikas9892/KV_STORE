#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

static int connect_to(const char* host, uint16_t port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { std::cerr << "socket: " << strerror(errno) << '\n'; return -1; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);

    if (::inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        hostent* he = ::gethostbyname(host);
        if (!he) {
            std::cerr << "Cannot resolve host: " << host << '\n';
            ::close(fd);
            return -1;
        }
        std::memcpy(&addr.sin_addr, he->h_addr_list[0], sizeof(in_addr));
    }

    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "connect: " << strerror(errno) << '\n';
        ::close(fd);
        return -1;
    }
    return fd;
}

static std::string recv_line(int fd) {
    std::string line;
    char        ch;
    while (::recv(fd, &ch, 1, 0) == 1) {
        if (ch == '\n') break;
        if (ch != '\r') line += ch;
    }
    return line;
}

static void print_response(const std::string& resp) {
    if (resp.size() > 1 && (resp[0] == '+' || resp[0] == '-'))
        std::cout << resp.substr(1) << '\n';
    else
        std::cout << resp << '\n';
}

int main(int argc, char* argv[]) {
    const char* host = (argc > 1) ? argv[1] : "127.0.0.1";
    uint16_t    port = (argc > 2) ? static_cast<uint16_t>(std::atoi(argv[2])) : 7379;

    int fd = connect_to(host, port);
    if (fd < 0) return 1;

    std::cout << "Connected to kv-store at " << host << ':' << port << "\n\n";
    std::cout << "Commands: PING | SET key value | GET key | DELETE key\n";
    std::cout << "          EXISTS key | SIZE | CLEAR\n";
    std::cout << "Press Ctrl+D or type QUIT to exit.\n\n";

    std::string line;
    while (true) {
        std::cout << "kv> ";
        std::cout.flush();

        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;

        std::string upper = line;
        for (char& c : upper) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
        if (upper == "QUIT" || upper == "EXIT") break;

        std::string req = line + "\r\n";
        if (::send(fd, req.data(), req.size(), 0) < 0) {
            std::cerr << "send failed: " << strerror(errno) << '\n';
            break;
        }

        std::string resp = recv_line(fd);
        if (resp.empty()) { std::cout << "(server closed connection)\n"; break; }
        print_response(resp);
    }

    std::cout << "Bye.\n";
    ::close(fd);
}
