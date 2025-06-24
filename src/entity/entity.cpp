#include "entity/entity.hpp"
#include "core/board.hpp"
#include "core/renderer.hpp"
#include "core/game.hpp" 

Entity::Entity(EntityType type, int x, int y, bool isPlayer, int health, Lane lane)
    : m_type(type), m_x(x), m_y(y), m_isPlayer(isPlayer), m_health(health), m_maxHealth(health), 
      m_moveTimer(0), m_moveSpeed(0), m_attackRange(1), m_damage(0), m_isFlying(false), m_canAttackAir(false),
      m_homeLane(lane) {
    if (x < 1 || x >= Renderer::BOARD_WIDTH - 1 || y < 1 || y >= Renderer::BOARD_HEIGHT - 1) {
        logWarning("Initial position out of bounds, clamping");
        m_x = std::max(1, std::min(x, Renderer::BOARD_WIDTH - 2));
        m_y = std::max(1, std::min(y, Renderer::BOARD_HEIGHT - 2));
    }
    if (health <= 0) {
        logWarning("Initial health invalid, setting to 1");
        m_health = 1;
    }
}

// Default update logic, calls the virtual move() function.
void Entity::update(const Board& board) {
    // Check for moveSpeed > 0 so that stationary entities don't waste cycles.
    if (m_moveSpeed > 0.0f) {
        m_moveTimer += 0.1f;
        if (m_moveTimer >= 1.0f / m_moveSpeed) {
            m_moveTimer = 0;
            move(board); 
        }
    }
}

void Entity::takeDamage(int damage) {
   if (damage < 0) {
        logWarning("Negative damage received");
        return;
    }
    m_health -= damage;
    if (m_health < 0) {
        m_health = 0;
    }
}

bool Entity::isAlive() const {
    return m_health > 0;
}

std::shared_ptr<Entity> Entity::findTarget(const Board& board) const {
    std::shared_ptr<Entity> bestTarget = nullptr;
    int closestDist = INT_MAX;
    
    for (const auto& entity : board.getAllEntities()) {
        if (!entity->isAlive() || entity->getIsPlayer() == m_isPlayer) continue;
        if (entity->isFlying() && !m_canAttackAir) continue;
        
        int dx = entity->getX() - m_x;
        int dy = entity->getY() - m_y;
        int dist = dx * dx + dy * dy;
        
        if (dist < closestDist) {
            closestDist = dist;
            bestTarget = entity;
        }
    }
    return bestTarget;
}

char Entity::getSymbol() const {
    return '?';
}