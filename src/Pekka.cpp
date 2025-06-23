#include "Pekka.hpp"
#include "game.hpp"

Pekka::Pekka(EntityType type, int x, int y, bool isPlayer, int health, Lane lane)
    : MovableEntity(type, x, y, isPlayer, health, lane) {
    calculateStats();
}

void Pekka::calculateStats() {
    m_moveSpeed = 0.75f;
    m_damage = 70;
    m_attackRange = 1;
    m_isFlying = false;
    m_canAttackAir = false;
}