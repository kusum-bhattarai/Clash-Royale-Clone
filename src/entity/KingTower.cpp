#include "entity/KingTower.hpp"

KingTower::KingTower(EntityType type, int x, int y, bool isPlayer, int health, Lane lane)
    : StationaryEntity(type, x, y, isPlayer, health, lane) {
    calculateStats();
}

// Specific implementation to calculate stats for king tower
void KingTower::calculateStats() {
    m_moveSpeed = 0.0f;
    m_damage = 75;
    m_attackRange = 7;
    m_isFlying = false;
    m_canAttackAir = true;
}