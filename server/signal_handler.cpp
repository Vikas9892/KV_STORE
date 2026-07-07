#include "server/signal_handler.h"
#include "utils/logger.h"

#include <csignal>
#include <stdexcept>
#include <unistd.h>

static int s_pipe_fds[2] = {-1, -1};

static void on_signal(int /*sig*/) {
    // Signal-safe: write() is async-signal-safe per POSIX.
    // A single byte wakes the select() in the accept loop.
    char byte = 1;
    (void)::write(s_pipe_fds[1], &byte, 1);
}

void SignalHandler::install() {
    if (::pipe(s_pipe_fds) != 0)
        throw std::runtime_error("SignalHandler: pipe() failed");

    struct sigaction sa{};
    sa.sa_handler = on_signal;
    ::sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    ::sigaction(SIGINT,  &sa, nullptr);
    ::sigaction(SIGTERM, &sa, nullptr);
}

bool SignalHandler::triggered() {
    if (s_pipe_fds[0] < 0) return false;
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(s_pipe_fds[0], &rfds);
    timeval tv{0, 0};
    return ::select(s_pipe_fds[0] + 1, &rfds, nullptr, nullptr, &tv) > 0;
}

int SignalHandler::pipe_read_fd() { return s_pipe_fds[0]; }
