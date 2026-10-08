#include "clash_royale/tui/input_handler.hpp"

#include <cctype>

#include <fcntl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

namespace cr {
namespace {

/// Puts the terminal into raw, non-echoing, non-blocking mode and restores the
/// previous settings on scope exit.
class TerminalSettings {
public:
    TerminalSettings() {
        tcgetattr(STDIN_FILENO, &m_old);
        m_raw = m_old;
        m_raw.c_lflag &= static_cast<tcflag_t>(~ICANON & ~ECHO);
        m_raw.c_cc[VMIN] = 0;
        m_raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &m_raw);
        fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    }

    ~TerminalSettings() {
        tcsetattr(STDIN_FILENO, TCSANOW, &m_old);
        fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) & ~O_NONBLOCK);
    }

    TerminalSettings(const TerminalSettings&) = delete;
    TerminalSettings& operator=(const TerminalSettings&) = delete;

private:
    struct termios m_old {};
    struct termios m_raw {};
};

}  // namespace

std::optional<char> InputHandler::getInput() {
    TerminalSettings settings;  // RAII: restored on scope exit

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);

    struct timeval tv {};
    tv.tv_sec = 0;
    tv.tv_usec = 100000;  // 100ms poll

    char input = 0;
    if (select(STDIN_FILENO + 1, &rfds, nullptr, nullptr, &tv) > 0 &&
        read(STDIN_FILENO, &input, 1) > 0) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(input)));
    }
    return std::nullopt;
}

}  // namespace cr
