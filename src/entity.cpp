#include "entity.hpp"
#include "board.hpp"

Entity::Entity(EntityType type, int x, int y, bool isPlayer, int health)
    : m_type(type), m_x(x), m_y(y), m_isPlayer(isPlayer), m_health(health), m_maxHealth(health), 
      m_moveTimer(0), m_moveSpeed(0), m_attackRange(1), m_damage(0), m_isFlying(false), m_canAttackAir(false) {
    m_isFlying = false;
    m_canAttackAir = false;
    calculateStats();
}

void Entity::calculateStats() {
    switch (m_type) {
        case EntityType::KNIGHT:
            m_moveSpeed = 1.0f;
            m_damage = 50;
            m_attackRange = 1;
            break;
        case EntityType::GOLEM:
            m_moveSpeed = 0.5f;
            m_damage = 50;
            m_attackRange = 1;
            break;
        case EntityType::PEKKA:
            m_moveSpeed = 0.75f;
            m_damage = 70;
            m_attackRange = 1;
            break;
        case EntityType::GOBLINS:
            m_moveSpeed = 1.2f;
            m_damage = 20;
            m_attackRange = 1;
            break;
        case EntityType::DRAGON:
            m_moveSpeed = 1.5f;
            m_damage = 50;
            m_attackRange = 3;  // Increased range for flying unit
            setIsFlying(true);
            setCanAttackAir(true);
            break;
        case EntityType::WIZARD:
            m_moveSpeed = 1.0f;
            m_damage = 65;
            m_attackRange = 4;  // Increased range for spellcaster
            setCanAttackAir(true);
            break;
        case EntityType::ARCHERS:
            m_moveSpeed = 1.2f;
            m_damage = 20;
            m_attackRange = 5;  // Longest range for archers
            setCanAttackAir(true);
            break;
        case EntityType::CANON:
            m_moveSpeed = 0.0f;
            m_damage = 40;
            m_attackRange = 5;
            break;
        case EntityType::KING_TOWER:
            m_moveSpeed = 0.0f;
            m_damage = 75;
            m_attackRange = 7;
            setCanAttackAir(true);
            break;
        case EntityType::QUEEN_TOWER:
            m_moveSpeed = 0.0f;
            m_damage = 50;
            m_attackRange = 5;
            setCanAttackAir(true);
            break;
    }
}

void Entity::update() {
    if (m_type != EntityType::KING_TOWER && m_type != EntityType::QUEEN_TOWER && m_type != EntityType::CANON) {
        m_moveTimer += 0.1f;
        if (m_moveTimer >= 1.0f / m_moveSpeed) {
            m_moveTimer = 0;
            move();
        }
    }
}

void Entity::move() {
    if (!isAlive() || m_type == EntityType::KING_TOWER || 
        m_type == EntityType::QUEEN_TOWER || m_type == EntityType::CANON) return;
    
    const Board* currentBoard = Board::getCurrentBoard();
    if (!currentBoard) return;
    
    auto target = findTarget(*currentBoard);
    
    // direction to target
    int dx = target.first - m_x;
    int dy = target.second - m_y;
    
    // distance to target
    int distance = std::abs(dx) + std::abs(dy);
    
    // movement patterns
    switch (m_type) {
        case EntityType::ARCHERS:
        case EntityType::WIZARD:
            // optimal attack range
            if (distance < m_attackRange - 1) {
                dx = -dx;  // Move away from target
                dy = -dy;
            } else if (distance > m_attackRange) {
                // move closer but stay at range
                if (std::abs(dx) > std::abs(dy)) {
                    m_x += (dx > 0) ? 1 : -1;
                } else {
                    m_y += (dy > 0) ? 1 : -1;
                }
            }
            break;
            
        case EntityType::DRAGON:
            // Flying unit - can move diagonally and over units
            if (distance > 2) {
                m_x += (dx > 0) ? 1 : (dx < 0 ? -1 : 0);
                m_y += (dy > 0) ? 1 : (dy < 0 ? -1 : 0);
            }
            break;
            
        case EntityType::GOBLINS:
            // Fast, erratic movement - zigzag pattern
            if (distance > 1) {
                // Goblins move in a zigzag pattern
                int movePattern = static_cast<int>(m_moveTimer * 10);
                if ((movePattern & 1) == 0) {
                    m_x += (dx > 0) ? 1 : -1;
                } else {
                    m_y += (dy > 0) ? 1 : -1;
                }
            }
            break;
            
        case EntityType::GOLEM:
            // Slow but steady, direct path to target
            if (distance > 1) {
                if (std::abs(dx) > std::abs(dy)) {
                    m_x += (dx > 0) ? 1 : -1;
                } else {
                    m_y += (dy > 0) ? 1 : -1;
                }
            }
            break;
            
        default:
            // Standard movement for other units
            if (distance > 1) {
                m_x += (dx > 0) ? 1 : (dx < 0 ? -1 : 0);
                m_y += (dy > 0) ? 1 : (dy < 0 ? -1 : 0);
            }
            break;
    }
    
    // Prioritize movement based on troop type
    switch (m_type) {
        case EntityType::GOLEM:
            // Golem moves straight towards buildings
            if (std::abs(dx) > std::abs(dy)) {
                m_x += (dx > 0) ? 1 : -1;
            } else {
                m_y += (dy > 0) ? 1 : -1;
            }
            break;
            
        case EntityType::DRAGON:
            // Dragon can move diagonally
            m_x += (dx > 0) ? 1 : (dx < 0 ? -1 : 0);
            m_y += (dy > 0) ? 1 : (dy < 0 ? -1 : 0);
            break;
            
        case EntityType::WIZARD:
        case EntityType::ARCHERS:
            // Ranged units try to maintain optimal distance
            if (distance < m_attackRange - 1) {
                m_x -= (dx > 0) ? 1 : -1;
                m_y -= (dy > 0) ? 1 : -1;
            } else if (distance > m_attackRange) {
                m_x += (dx > 0) ? 1 : -1;
                m_y += (dy > 0) ? 1 : -1;
            }
            break;
            
        default:
            // Other troops move normally towards target
            if (dx != 0) m_x += (dx > 0) ? 1 : -1;
            if (dy != 0) m_y += (dy > 0) ? 1 : -1;
    }
    
    // Boundary checks
    if (m_x < 1) m_x = 1;
    if (m_x > 28) m_x = 28;
    if (m_y < 1) m_y = 1;
    if (m_y > 28) m_y = 28;
}

void Entity::takeDamage(int damage) {
    m_health -= damage;
    if (m_health < 0) m_health = 0;
}

bool Entity::isAlive() const {
    return m_health > 0;
}

std::pair<int, int> Entity::findTarget(const Board& board) const {
    std::pair<int, int> target = {m_x, m_isPlayer ? m_y + 1 : m_y - 1};  // Default move forward
    int closestDist = 999999;
    
    for (const auto& entity : board.getAllEntities()) {
        if (!entity->isAlive() || entity->getIsPlayer() == m_isPlayer) continue;
        if (entity->isFlying() && !canAttackAir()) continue;
        
        // Calculate distance
        int dx = entity->getX() - m_x;
        int dy = entity->getY() - m_y;
        int dist = dx * dx + dy * dy;
        
        // Check if this is a closer valid target
        if (dist < closestDist && isValidTarget(*entity)) {
            closestDist = dist;
            target = {entity->getX(), entity->getY()};
        }
    }
    
    return target;
}

bool Entity::isValidTarget(const Entity& target) const {
    // Towers prioritize any enemy unit in range
    if (m_type == EntityType::KING_TOWER || m_type == EntityType::QUEEN_TOWER) {
        return true;
    }
    
    // Ground units can't attack air units unless they have canAttackAir capability
    if (target.isFlying() && !canAttackAir()) {
        return false;
    }
    
    // Most troops prioritize buildings
    if (target.getType() == EntityType::KING_TOWER || 
        target.getType() == EntityType::QUEEN_TOWER || 
        target.getType() == EntityType::CANON) {
        return true;
    }
    
    // Otherwise, target any enemy unit
    return true;
}

char Entity::getSymbol() const {
    switch (m_type) {
        case EntityType::KING_TOWER:
            return m_isPlayer ? 'T' : 't';
        case EntityType::QUEEN_TOWER:
            return m_isPlayer ? 'Q' : 'q';
        case EntityType::KNIGHT:
            return m_isPlayer ? 'K' : 'k';
        case EntityType::GOLEM:
            return m_isPlayer ? 'G' : 'g';
        case EntityType::PEKKA:
            return m_isPlayer ? 'P' : 'p';
        case EntityType::GOBLINS:
            return m_isPlayer ? 'B' : 'b';
        case EntityType::DRAGON:
            return m_isPlayer ? 'D' : 'd';
        case EntityType::WIZARD:
            return m_isPlayer ? 'W' : 'w';
        case EntityType::ARCHERS:
            return m_isPlayer ? 'A' : 'a';
        case EntityType::CANON:
            return m_isPlayer ? 'C' : 'c';
        default:
            return '?';
    }
}