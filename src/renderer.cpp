#include "renderer.hpp"
#include <iostream>
#include <iomanip>

Renderer::Renderer() {
    try {
        buffer = std::vector<std::string>(BOARD_HEIGHT);
        for (auto& row : buffer) {
            row = std::string(BOARD_WIDTH, ' ');
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
        row = std::string(BOARD_WIDTH, ' ');
    }
    drawBorders();
}

void Renderer::drawBoard(const Board& board) {
    const auto& entities = board.getEntities();
    
    for (const auto& entity : entities) {
        if (!entity->isAlive()) continue;
        
        int x = entity->getX();
        int y = entity->getY();
        
        if (x >= 0 && x < BOARD_WIDTH && y >= 0 && y < BOARD_HEIGHT) {
            buffer[y][x] = entity->getSymbol();
            drawHealthBar(x, y + 1, entity->getHealth(), 
                         entity->getType() == EntityType::KING_TOWER ? 4000 : 
                         entity->getType() == EntityType::QUEEN_TOWER ? 1500 : 600);
        }
    }
}

void Renderer::drawHealthBar(int x, int y, int health, int maxHealth) {
    if (y >= BOARD_HEIGHT || y < 0 || x >= BOARD_WIDTH || x < 0) return;
    
    const int barLength = 5;
    const int maxBarWidth = BOARD_WIDTH - x;
    if (maxBarWidth <= 0) return;
    
    int filledLength = std::max(0, std::min(barLength, (health * barLength) / maxHealth));
    
    std::string healthBar = "[";
    for (int i = 0; i < barLength && x + i + 2 < BOARD_WIDTH; i++) {
        healthBar += (i < filledLength) ? '|' : '-';
    }
    healthBar += "]";
    
    int barX = x - 2;
    int barY = (y >= BOARD_HEIGHT - 5) ? y - 1 : y + 1;
    
    if (barX + healthBar.length() <= BOARD_WIDTH && barX >= 0) {
        for (size_t i = 0; i < healthBar.length(); i++) {
            buffer[barY][barX + i] = healthBar[i];
        }
    }
}

void Renderer::drawStatus(float elixirPlayerOne, float elixirPlayerTwo, bool isPlayerOneTurn, float gameTimer) {
    int timeLeft = static_cast<int>(120 - gameTimer);
    std::string status = "Time: " + std::to_string(timeLeft) + "s   P1 Elixir: " + 
                        std::to_string(static_cast<int>(elixirPlayerOne)) +
                        "   P2 Elixir: " + std::to_string(static_cast<int>(elixirPlayerTwo));
    
    // Center the status text
    while (status.length() < BOARD_WIDTH) {
        status = " " + status + " ";
    }
    if (status.length() > BOARD_WIDTH) {
        status = status.substr(0, BOARD_WIDTH);
    }
    
    buffer[BOARD_HEIGHT - 1] = status;
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
    for (int x = 0; x < BOARD_WIDTH; x++) {
        buffer[0][x] = '-';
        buffer[BOARD_HEIGHT - 2][x] = '-';
    }
    
    for (int y = 0; y < BOARD_HEIGHT - 1; y++) {
        buffer[y][0] = '|';
        buffer[y][BOARD_WIDTH - 1] = '|';
    }
    
    // Player 2 is at the top, Player 1 is at the bottom
    std::string p2Label = "PLAYER 2 (AI)";
    std::string p1Label = "PLAYER 1 (YOU)";
    
    int labelPosP2 = (BOARD_WIDTH - p2Label.length()) / 2;
    for(size_t i = 0; i < p2Label.length(); i++) {
        buffer[1][labelPosP2 + i] = p2Label[i];
    }
    
    int labelPosP1 = (BOARD_WIDTH - p1Label.length()) / 2;
    for(size_t i = 0; i < p1Label.length(); i++) {
        buffer[BOARD_HEIGHT - 3][labelPosP1 + i] = p1Label[i];
    }
}