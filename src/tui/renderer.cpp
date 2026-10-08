#include "clash_royale/tui/renderer.hpp"
#include <iostream>
#include <algorithm>
#include <iomanip>

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
            drawHealthBar(x, y + 1, entity->getHealth(), entity->getMaxHealth());
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
    if (y >= kArenaHeight || y < 0 || x >= kArenaWidth || x < 0) return;
    
    const int barLength = 5;
    const int maxBarWidth = kArenaWidth - x;
    if (maxBarWidth <= 0) return;
    
    int filledLength = std::max(0, std::min(barLength, (health * barLength) / maxHealth));
    
    std::string healthBar = "[";
    for (int i = 0; i < barLength && x + i + 2 < kArenaWidth; i++) {
        healthBar += (i < filledLength) ? '|' : '-';
    }
    healthBar += "]";
    
    int barX = x - 2;
    int barY = (y >= kArenaHeight - 5) ? y - 1 : y + 1;
    
    if (barX + healthBar.length() <= kArenaWidth && barX >= 0) {
        for (size_t i = 0; i < healthBar.length(); i++) {
            buffer[barY][barX + i] = healthBar[i];
        }
    }
}

void Renderer::drawStatus(float elixirPlayerOne, float elixirPlayerTwo, float gameTimer) {
    int timeLeft = static_cast<int>(120 - gameTimer);
    std::string status = "Time: " + std::to_string(timeLeft) + "s   P1 Elixir: " + 
                        std::to_string(static_cast<int>(elixirPlayerOne)) +
                        "   P2 Elixir: " + std::to_string(static_cast<int>(elixirPlayerTwo));
    
    // Center the status text
    while (status.length() < kArenaWidth) {
        status = " " + status + " ";
    }
    if (status.length() > kArenaWidth) {
        status = status.substr(0, kArenaWidth);
    }
    
    buffer[kArenaHeight - 1] = status;
}

void Renderer::drawPrompt(const std::string& message) {
    std::string prompt = message;
    while (prompt.length() < kArenaWidth) {
        prompt = " " + prompt + " ";
    }
    if (prompt.length() > kArenaWidth) {
        prompt = prompt.substr(0, kArenaWidth);
    }
    buffer[kArenaHeight - 2] = prompt;
}


void Renderer::display() {
    std::cout << "\033[H";
    for (const auto& row : buffer) {
        std::cout << row << "\n";
    }
    std::cout << "\n╔════════════════ CONTROLS ════════════════╗\n";
    std::cout << "║ K=Knight(4)  G=Golem(5)   P=Pekka(4)    ║\n";
    std::cout << "║ B=Goblins(3) D=Dragon(5)  W=Wizard(4)   ║\n";
    std::cout << "║ A=Archers(2) C=Canon(3)   Q=Quit Game   ║\n";
    std::cout << "╚═══════════════════════════════════════════╝\n";
    std::cout.flush();
}

void Renderer::drawBorders() {
    for (int x = 0; x < kArenaWidth; x++) {
        buffer[0][x] = '-';
        buffer[kArenaHeight - 2][x] = '-';
    }
    
    for (int y = 0; y < kArenaHeight - 1; y++) {
        buffer[y][0] = '|';
        buffer[y][kArenaWidth - 1] = '|';
    }
    
    // Player 2 is at the top, Player 1 is at the bottom
    std::string p2Label = "PLAYER 2 (AI)";
    std::string p1Label = "PLAYER 1 (YOU)";
    
    int labelPosP2 = (kArenaWidth - p2Label.length()) / 2;
    for(size_t i = 0; i < p2Label.length(); i++) {
        buffer[1][labelPosP2 + i] = p2Label[i];
    }
    
    int labelPosP1 = (kArenaWidth - p1Label.length()) / 2;
    for(size_t i = 0; i < p1Label.length(); i++) {
        buffer[kArenaHeight - 3][labelPosP1 + i] = p1Label[i];
    }
}

}  // namespace cr
