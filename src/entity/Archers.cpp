#include "entity/Archers.hpp"
#include "core/game.hpp" 

Archers::Archers(EntityType type, int x, int y, bool isPlayer, int health, Lane lane)
    : RangedEntity(type, x, y, isPlayer, health, lane) {
    calculateStats();
}

void Archers::calculateStats() {
    m_moveSpeed = 1.2f;
    m_damage = 20;
    m_attackRange = 7; 
    m_isFlying = false;
    m_canAttackAir = true; 
}