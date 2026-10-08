#include <iostream>

#include "clash_royale/tui/game.hpp"

int main() {
    try {
        cr::Game game;
        game.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
