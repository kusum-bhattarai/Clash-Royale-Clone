#include "entity/Knight.hpp"
#include "core/game.hpp"

Knight::Knight(EntityType type, int x, int y, bool isPlayer, int health, Lane lane)
    : MovableEntity(type, x, y, isPlayer, health, lane) {
    calculateStats();
}

void Knight::calculateStats() {
    m_moveSpeed = 1.0f;
    m_damage = 50;
    m_attackRange = 1;
    m_isFlying = false;
    m_canAttackAir = false; 
}