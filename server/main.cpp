#include "server/server.h"

#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) {
    uint16_t port = (argc > 1) ? static_cast<uint16_t>(std::stoi(argv[1])) : 7379;
    try {
        TcpServer server(ServerConfig{port});
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "[FATAL] " << e.what() << '\n';
        return 1;
    }
}
