#pragma once
#include "clash_royale/core/types.hpp"
#include "clash_royale/sim/board.hpp"
#include <string>
#include <vector>

namespace cr {

class Renderer {
public:
    Renderer();
    void clear();
    void drawBoard(const Board& board);
    void drawStatus(float elixirPlayerOne, float elixirPlayerTwo, float gameTimer = 0.0f);
    void drawPrompt(const std::string& message);
    void display();

private:
    std::vector<std::string> buffer;
    
    void drawHealthBar(int x, int y, int health, int maxHealth);
    void drawBorders();
};

}  // namespace cr
