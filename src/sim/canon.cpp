#include "clash_royale/sim/canon.hpp"

namespace cr {

Canon::Canon(EntityType type, int x, int y, bool isPlayer, int health, Lane lane)
    : StationaryEntity(type, x, y, isPlayer, health, lane) {
    calculateStats();
}

void Canon::calculateStats() {
    m_moveSpeed = 0.0f;
    m_damage = 40;
    m_attackSpeed = 1.0f / 0.8f;  // 0.8s hit speed
    m_attackRange = 5;
    m_isFlying = false;
    setCanAttackAir(false); 
}

}  // namespace cr
