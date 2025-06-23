#include "QueenTower.hpp"

QueenTower::QueenTower(EntityType type, int x, int y, bool isPlayer, int health, Lane lane)
    : StationaryEntity(type, x, y, isPlayer, health, lane) {
    calculateStats();
}

void QueenTower::calculateStats() {
    m_moveSpeed = 0.0f;
    m_damage = 50;
    m_attackRange = 5;
    m_isFlying = false;
    m_canAttackAir = true;
}