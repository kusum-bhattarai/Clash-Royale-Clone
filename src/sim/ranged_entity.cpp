#include "clash_royale/sim/ranged_entity.hpp"
#include "clash_royale/sim/board.hpp"
#include <memory> 

namespace cr {

void RangedEntity::move(const Board& board) {
    std::shared_ptr<Entity> target = findTarget(board);
    if (target == nullptr) return; // No target, do nothing.

    int dx = target->getX() - m_x;
    int dy = target->getY() - m_y;
    int distanceSq = dx * dx + dy * dy;

    if (distanceSq <= m_attackRange * m_attackRange) {
        return;
    }
    MovableEntity::move(board);
}

}  // namespace cr
