#include "TowerPrioritizingEntity.hpp"
#include "board.hpp"
#include <climits> 
#include <memory>  

// This is the Golem's "brain". It finds the closest tower to attack.
std::shared_ptr<Entity> TowerPrioritizingEntity::findTarget(const Board& board) const {
    std::shared_ptr<Entity> bestTarget = nullptr;
    int closestDistSq = INT_MAX;

    for (const auto& potentialTarget : board.getAllEntities()) {
        if (!potentialTarget->isAlive() || potentialTarget->getIsPlayer() == m_isPlayer) {
            continue;
        }

        EntityType targetType = potentialTarget->getType();

        // The Golem ONLY considers these three types as valid targets.
        if (targetType == EntityType::KING_TOWER || 
            targetType == EntityType::QUEEN_TOWER || 
            targetType == EntityType::CANON) {
            
            int dx = potentialTarget->getX() - m_x;
            int dy = potentialTarget->getY() - m_y;
            int distSq = dx * dx + dy * dy;

            if (distSq < closestDistSq) {
                closestDistSq = distSq;
                bestTarget = potentialTarget;
            }
        }
    }
    return bestTarget;
}