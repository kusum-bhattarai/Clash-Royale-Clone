#pragma once

#include <optional>

namespace cr {

/// Non-blocking keyboard polling for the terminal front-end.
///
/// The POSIX implementation (termios, fcntl, select) lives entirely in the
/// corresponding .cpp. Previously it sat inline in this header, so every
/// translation unit that transitively included it -- which, via the old
/// EntityFactory -> Game -> InputHandler chain, meant most of the simulation --
/// pulled in <termios.h> and could not be compiled on a non-POSIX platform.
class InputHandler {
public:
    /// Polls stdin for up to 100ms.
    ///
    /// Returns the lowercased key that was pressed, or nullopt if the poll
    /// timed out. The terminal is switched to raw, non-echoing mode for the
    /// duration of the call and restored before returning.
    std::optional<char> getInput();
};

}  // namespace cr
