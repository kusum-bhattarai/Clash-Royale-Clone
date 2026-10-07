#include "clash_royale/sim/pekka.hpp"

namespace cr {

Pekka::Pekka(EntityType type, int x, int y, bool isPlayer, int health, Lane lane)
    : MovableEntity(type, x, y, isPlayer, health, lane) {
    calculateStats();
}

void Pekka::calculateStats() {
    m_moveSpeed = 0.75f;
    m_damage = 70;
    m_attackSpeed = 1.0f / 1.8f;  // 1.8s hit speed, slow but heavy
    m_attackRange = 1;
    m_isFlying = false;
    m_canAttackAir = false;
}

}  // namespace cr
