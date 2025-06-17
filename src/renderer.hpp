#pragma once
#include "board.hpp"
#include <string>
#include <vector>

class Renderer {
public:
    Renderer();
    void clear();
    void drawBoard(const Board& board);
    void drawStatus(float elixirPlayerOne, float elixirPlayerTwo, bool isPlayerOneTurn, float gameTimer = 0.0f);
    void display();
    static const int BOARD_WIDTH = 40;
    static const int BOARD_HEIGHT = 35;

private:
    std::vector<std::string> buffer;
    
    void drawHealthBar(int x, int y, int health, int maxHealth);
    void drawBorders();
};
