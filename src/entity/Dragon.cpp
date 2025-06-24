#include "entity/Dragon.hpp"
#include "core/game.hpp"
#include "core/board.hpp"
#include "core/renderer.hpp"
#include <memory>

Dragon::Dragon(EntityType type, int x, int y, bool isPlayer, int health, Lane lane)
    : MovableEntity(type, x, y, isPlayer, health, lane) {
    calculateStats();
}

void Dragon::calculateStats() {
    m_moveSpeed = 1.5f;
    m_damage = 50;
    m_attackRange = 3;
    m_isFlying = true;      // It's a flying unit
    m_canAttackAir = true;
}

// The Dragon's unique movement logic allows it to move diagonally.
void Dragon::move(const Board& board) {
    std::shared_ptr<Entity> target = findTarget(board);
    if (target == nullptr) return;

    int dx = target->getX() - m_x;
    int dy = target->getY() - m_y;

    // Move one step on both axes if possible (diagonal movement).
    if (dx != 0) {
        m_x += (dx > 0) ? 1 : -1;
    }
    if (dy != 0) {
        m_y += (dy > 0) ? 1 : -1;
    }

    // Boundary checks
    m_x = std::max(1, std::min(m_x, Renderer::BOARD_WIDTH - 2));
    m_y = std::max(1, std::min(m_y, Renderer::BOARD_HEIGHT - 2));
}