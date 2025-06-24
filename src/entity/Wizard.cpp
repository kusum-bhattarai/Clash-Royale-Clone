#include "entity/Wizard.hpp"
#include "core/game.hpp"

Wizard::Wizard(EntityType type, int x, int y, bool isPlayer, int health, Lane lane)
    : RangedEntity(type, x, y, isPlayer, health, lane) {
    calculateStats();
}

void Wizard::calculateStats() {
    m_moveSpeed = 1.0f;
    m_damage = 65;     
    m_attackRange = 5;  
    m_isFlying = false;
    m_canAttackAir = true;
}