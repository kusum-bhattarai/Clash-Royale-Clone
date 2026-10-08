#include "clash_royale/tui/renderer.hpp"
#include <iostream>
#include <algorithm>
#include <cstddef>
#include <string>

namespace cr {

Renderer::Renderer() {
    try {
        buffer = std::vector<std::string>(kArenaHeight);
        for (auto& row : buffer) {
            row = std::string(kArenaWidth, ' ');
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize renderer: " << e.what() << std::endl;
        throw;
    }
}

void Renderer::clear() {
    std::cout << "\033[2J\033[H";
    std::cout << "\033[?25l";
    for (auto& row : buffer) {
        row = std::string(kArenaWidth, ' ');
    }
    drawBorders();
}

void Renderer::drawBoard(const Board& board) {
    drawTerrain(board.arena());

    const auto& entities = board.getEntities();
    
    for (const auto& entity : entities) {
        if (!entity->isAlive()) continue;
        
        int x = entity->getX();
        int y = entity->getY();
        
        if (x >= 0 && x < kArenaWidth && y >= 0 && y < kArenaHeight) {
            buffer[y][x] = entity->getSymbol();
            // Previously this guessed max health from the entity type, using a
            // literal 600 for every troop -- so a 200 HP Goblin rendered a bar
            // that never dropped below two thirds, and a 120 HP Archer's bar
            // barely moved. Entities know their own maximum.
            drawHealthBar(x, y, entity->getHealth(), entity->getMaxHealth());
        }
    }
}

void Renderer::drawTerrain(const Arena& arena) {
    // Drawn before the entities so units and their health bars sit on top.
    const int width = std::min(arena.width(), kArenaWidth);
    const int height = std::min(arena.height(), kArenaHeight);

    for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
            switch (arena.tile(x, y)) {
                case Tile::Water:
                    buffer[y][x] = '~';
                    break;
                case Tile::Bridge:
                    buffer[y][x] = '=';
                    break;
                case Tile::Blocked:
                    buffer[y][x] = '#';
                    break;
                case Tile::Ground:
                    break;
            }
        }
    }
}

void Renderer::drawHealthBar(int x, int y, int health, int maxHealth) {
    if (maxHealth <= 0 || !insideFrame(x, y)) {
        return;
    }

    const int barLength = 5;
    const int filled = std::max(0, std::min(barLength, (health * barLength) / maxHealth));

    std::string bar = "[";
    for (int i = 0; i < barLength; ++i) {
        bar += (i < filled) ? '|' : '-';
    }
    bar += ']';

    // Centre the bar on its unit. The bar is barLength + 2 characters wide, so
    // it starts half that to the left. It used to start at x - 2, which left it
    // sitting one column right of whatever it belonged to.
    const int width = static_cast<int>(bar.size());
    int barX = x - width / 2;
    barX = std::max(1, std::min(barX, kArenaWidth - 1 - width));

    // Put the bar on the side away from the middle of the arena, so the two
    // halves mirror each other. Drawing it below everything made player two's
    // towers look a row closer to the river than player one's.
    const int middle = kArenaHeight / 2;
    const int barY = (y < middle) ? y - 1 : y + 1;
    if (!insideFrame(barX, barY)) {
        return;
    }

    for (int i = 0; i < width; ++i) {
        buffer[static_cast<std::size_t>(barY)][static_cast<std::size_t>(barX + i)] = bar[static_cast<std::size_t>(i)];
    }
}

void Renderer::drawStatus(float elixirPlayerOne, float elixirPlayerTwo, float gameTimer) {
    const int timeLeft = std::max(0, static_cast<int>(120 - gameTimer));
    m_status = "Time: " + std::to_string(timeLeft) + "s   P1 Elixir: " +
               std::to_string(static_cast<int>(elixirPlayerOne)) + "   P2 Elixir: " +
               std::to_string(static_cast<int>(elixirPlayerTwo));
}

void Renderer::drawPrompt(const std::string& message) {
    m_prompt = message;
}

void Renderer::display() {
    std::cout << "\033[H";
    for (const auto& row : buffer) {
        std::cout << row << "\n";
    }

    // Printed under the arena rather than written into it, so neither can
    // overwrite the frame or a unit.
    std::cout << centred(m_status) << "\n";
    std::cout << centred(m_prompt) << "\n";

    std::cout << "\n";
    std::cout << "\u2554\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550"
                 " CONTROLS "
                 "\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2557\n";
    std::cout << "\u2551" "K=Knight(4)  " "G=Golem(5)   " "P=Pekka(4)   " "\u2551\n";
    std::cout << "\u2551" "B=Goblins(3) " "D=Dragon(5)  " "W=Wizard(4)  " "\u2551\n";
    std::cout << "\u2551" "A=Archers(2) " "C=Canon(3)   " "Q=Quit Game  " "\u2551\n";
    std::cout << "\u255A";
    for (int i = 0; i < kArenaWidth - 2; ++i) {
        std::cout << "\u2550";
    }
    std::cout << "\u255D\n";
    std::cout.flush();
}

std::string Renderer::centred(const std::string& text) const {
    // Anything too wide is printed in full rather than cut. Truncating left the
    // lane prompt reading "(X) to ca".
    if (static_cast<int>(text.size()) >= kArenaWidth) {
        return text;
    }
    const int pad = (kArenaWidth - static_cast<int>(text.size())) / 2;
    return std::string(static_cast<std::size_t>(pad), ' ') + text;
}

void Renderer::drawBorders() {
    // The frame occupies the full buffer: top and bottom rows, first and last
    // columns. That puts its centre on the arena's centre, which is where the
    // river is. The bottom edge used to sit two rows early, so the river was
    // half a row below the middle of the frame, and the prompt was written over
    // the border besides.
    const int lastRow = kArenaHeight - 1;
    const int lastColumn = kArenaWidth - 1;

    for (int x = 0; x < kArenaWidth; ++x) {
        buffer[0][static_cast<std::size_t>(x)] = '-';
        buffer[static_cast<std::size_t>(lastRow)][static_cast<std::size_t>(x)] = '-';
    }
    for (int y = 0; y <= lastRow; ++y) {
        buffer[static_cast<std::size_t>(y)][0] = '|';
        buffer[static_cast<std::size_t>(y)][static_cast<std::size_t>(lastColumn)] = '|';
    }

    centreText(1, "PLAYER 2 (AI)");
    centreText(lastRow - 1, "PLAYER 1 (YOU)");
}

bool Renderer::insideFrame(int x, int y) const {
    return x >= 1 && x <= kArenaWidth - 2 && y >= 1 && y <= kArenaHeight - 2;
}

void Renderer::centreText(int row, const std::string& text) {
    if (row < 0 || row >= kArenaHeight) {
        return;
    }
    const int start = (kArenaWidth - static_cast<int>(text.size())) / 2;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const int x = start + static_cast<int>(i);
        if (x >= 0 && x < kArenaWidth) {
            buffer[static_cast<std::size_t>(row)][static_cast<std::size_t>(x)] = text[i];
        }
    }
}

}  // namespace cr
