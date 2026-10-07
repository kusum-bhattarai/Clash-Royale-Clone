#include "clash_royale/sim/golem.hpp"

namespace cr {

Golem::Golem(EntityType type, int x, int y, bool isPlayer, int health, Lane lane)
    : TowerPrioritizingEntity(type, x, y, isPlayer, health, lane) {
    calculateStats();
}

void Golem::calculateStats() {
    m_moveSpeed = 0.5f; // Golem is slow
    m_damage = 50;
    m_attackSpeed = 1.0f / 2.5f;  // 2.5s hit speed, very slow
    m_attackRange = 1;
    m_isFlying = false;
    m_canAttackAir = false;
}

}  // namespace cr
