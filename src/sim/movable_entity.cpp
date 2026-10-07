#include "clash_royale/sim/movable_entity.hpp"
#include "clash_royale/sim/board.hpp"

namespace cr {

// This is the standard movement logic for a basic melee troop.
void MovableEntity::move(const Board& board) {
    auto targetPos = findTarget(board);
    if (targetPos == nullptr) return; // No target found, do nothing.

    int dx = targetPos->getX() - m_x;
    int dy = targetPos->getY() - m_y;

    // Move one step closer to the target
    if (std::abs(dx) > std::abs(dy)) {
        m_x += (dx > 0) ? 1 : -1;
    } else {
        m_y += (dy > 0) ? 1 : -1;
    }

    // Boundary checks
    m_x = std::max(1, std::min(m_x, kArenaWidth - 2));
    m_y = std::max(1, std::min(m_y, kArenaHeight - 2));
}

}  // namespace cr
