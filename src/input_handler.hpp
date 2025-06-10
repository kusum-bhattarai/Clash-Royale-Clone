// input_handler.hpp
#pragma once
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <fcntl.h>
#include <optional>

class TerminalSettings {
public:
    TerminalSettings() {
        tcgetattr(STDIN_FILENO, &old_tio_);
        new_tio_ = old_tio_;
        new_tio_.c_lflag &= (~ICANON & ~ECHO); 
        new_tio_.c_cc[VMIN] = 0;
        new_tio_.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &new_tio_);
        fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    }

    ~TerminalSettings() {
        tcsetattr(STDIN_FILENO, TCSANOW, &old_tio_);
        fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) & ~O_NONBLOCK);
    }

private:
    struct termios old_tio_, new_tio_;
};

class InputHandler {
public:
    std::optional<char> getInput() {
        TerminalSettings settings; // RAII: settings restored on scope exit
        fd_set rfds;
        struct timeval tv;
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        tv.tv_sec = 0;
        tv.tv_usec = 100000; // 100ms timeout

        char input;
        if (select(STDIN_FILENO + 1, &rfds, nullptr, nullptr, &tv) > 0 &&
            read(STDIN_FILENO, &input, 1) > 0) {
            return std::tolower(input);
        }
        return std::nullopt;
    }
};