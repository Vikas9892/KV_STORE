#pragma once

// Self-pipe trick for signal-safe server shutdown.
//
// SignalHandler::install() registers SIGINT and SIGTERM handlers that
// write one byte to a pipe. TcpServer::run() selects on both the
// listening socket and the read-end of the pipe; when the pipe becomes
// readable it knows a signal arrived and exits the accept loop cleanly.
//
// Usage:
//   SignalHandler::install();
//   ...
//   // In accept loop:
//   if (SignalHandler::triggered()) break;
//   // Or pass pipe_read_fd() to select/epoll

class SignalHandler {
public:
    static void install();         // call once from main()
    static bool triggered();       // true after SIGINT / SIGTERM
    static int  pipe_read_fd();    // add to select/epoll watch set

    SignalHandler()                              = delete;
    SignalHandler(const SignalHandler&)          = delete;
    SignalHandler& operator=(SignalHandler&)     = delete;
};
