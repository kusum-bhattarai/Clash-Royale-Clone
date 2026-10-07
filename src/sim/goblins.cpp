#include "clash_royale/sim/goblins.hpp"
#include "clash_royale/sim/board.hpp"
#include <memory>

namespace cr {

Goblins::Goblins(EntityType type, int x, int y, bool isPlayer, int health, Lane lane)
    : MovableEntity(type, x, y, isPlayer, health, lane) {
    calculateStats();
}

void Goblins::calculateStats() {
    m_moveSpeed = 1.2f;
    m_damage = 20;
    m_attackSpeed = 1.0f / 1.1f;  // 1.1s hit speed, fast
    m_attackRange = 1;
    m_isFlying = false;
    m_canAttackAir = false;
}

// Goblins zigzag: they alternate which axis they close on, instead of
// exhausting one axis before starting the other.
//
// This never actually happened before. The branch was keyed on `m_moveTimer`,
// but Entity::update() zeroes that on the line *before* it calls move(), so the
// timer was always 0.0 here, `movePattern` was always 0, and only the
// horizontal-first branch ever ran. It now keys on m_stepCount, which advances
// once per movement step.
void Goblins::move(const Board& board) {
    std::shared_ptr<Entity> target = findTarget(board);
    if (target == nullptr) {
        return;
    }

    const int dx = target->getX() - m_x;
    const int dy = target->getY() - m_y;

    const bool horizontalFirst = (m_stepCount % 2) == 0;
    if (horizontalFirst) {
        if (dx != 0) {
            m_x += (dx > 0) ? 1 : -1;
        } else if (dy != 0) {
            m_y += (dy > 0) ? 1 : -1;
        }
    } else {
        if (dy != 0) {
            m_y += (dy > 0) ? 1 : -1;
        } else if (dx != 0) {
            m_x += (dx > 0) ? 1 : -1;
        }
    }

    m_x = std::max(1, std::min(m_x, kArenaWidth - 2));
    m_y = std::max(1, std::min(m_y, kArenaHeight - 2));
}

}  // namespace cr
