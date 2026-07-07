#include "config/config.h"
#include "server/server.h"
#include "utils/logger.h"

#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) {
    const std::string cfg_path = (argc > 1) ? argv[1] : "config.json";
    Config cfg = Config::from_file(cfg_path);
    try {
        TcpServer server(cfg);
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "[FATAL] " << e.what() << '\n';
        return 1;
    }
}
