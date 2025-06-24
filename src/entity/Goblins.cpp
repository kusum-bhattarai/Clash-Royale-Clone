#include "Goblins.hpp"
#include "game.hpp"
#include "board.hpp"
#include "renderer.hpp"
#include <memory>

Goblins::Goblins(EntityType type, int x, int y, bool isPlayer, int health, Lane lane)
    : MovableEntity(type, x, y, isPlayer, health, lane) {
    calculateStats();
}

void Goblins::calculateStats() {
    m_moveSpeed = 1.2f;
    m_damage = 20;
    m_attackRange = 1;
    m_isFlying = false;
    m_canAttackAir = false;
}

// Goblins have a unique zigzag movement pattern.
void Goblins::move(const Board& board) {
    std::shared_ptr<Entity> target = findTarget(board);
    if (target == nullptr) return;

    int dx = target->getX() - m_x;
    int dy = target->getY() - m_y;

    // Fast, erratic movement - zigzag pattern
    int movePattern = static_cast<int>(m_moveTimer * 10);
    if ((movePattern & 1) == 0) { // On even ticks, move horizontally if possible
        if (dx != 0) m_x += (dx > 0) ? 1 : -1;
        else if (dy != 0) m_y += (dy > 0) ? 1 : -1;
    } else { // On odd ticks, move vertically if possible
        if (dy != 0) m_y += (dy > 0) ? 1 : -1;
        else if (dx != 0) m_x += (dx > 0) ? 1 : -1;
    }

    // Boundary checks
    m_x = std::max(1, std::min(m_x, Renderer::BOARD_WIDTH - 2));
    m_y = std::max(1, std::min(m_y, Renderer::BOARD_HEIGHT - 2));
}