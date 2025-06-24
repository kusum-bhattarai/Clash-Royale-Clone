#include "entity/Canon.hpp"

Canon::Canon(EntityType type, int x, int y, bool isPlayer, int health, Lane lane)
    : StationaryEntity(type, x, y, isPlayer, health, lane) {
    calculateStats();
}

void Canon::calculateStats() {
    m_moveSpeed = 0.0f;
    m_damage = 40;
    m_attackRange = 5;
    m_isFlying = false;
    setCanAttackAir(false); 
}