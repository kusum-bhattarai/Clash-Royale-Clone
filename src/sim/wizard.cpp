#include "clash_royale/sim/wizard.hpp"

namespace cr {

Wizard::Wizard(EntityType type, int x, int y, bool isPlayer, int health, Lane lane)
    : RangedEntity(type, x, y, isPlayer, health, lane) {
    calculateStats();
}

void Wizard::calculateStats() {
    m_moveSpeed = 1.0f;
    m_damage = 65;     
    m_attackSpeed = 1.0f / 1.4f;  // 1.4s hit speed
    m_attackRange = 5;  
    m_isFlying = false;
    m_canAttackAir = true;
}

}  // namespace cr
