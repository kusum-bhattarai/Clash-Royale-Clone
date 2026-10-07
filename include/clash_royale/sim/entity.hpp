#pragma once
#include "clash_royale/core/types.hpp"
#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include <utility>
#include <iostream>

namespace cr {

class Board;

class Entity {
public:
    Entity(EntityType type, int x, int y, bool isPlayer, int health, Lane lane);
    virtual ~Entity() = default;
    /// Advances this entity by `dt` seconds, stepping it once its move timer
    /// reaches the interval implied by its speed.
    virtual void update(const Board& board, float dt = kDefaultTimeStep);
    void takeDamage(int damage);
    bool isAlive() const;

    EntityType getType() const { return m_type; }
    int getX() const { return m_x; }
    int getY() const { return m_y; }
    int getHealth() const { return m_health; }
    int getMaxHealth() const { return m_maxHealth; }
    bool getIsPlayer() const { return m_isPlayer; }
    bool isFlying() const { return m_isFlying; }
    int getAttackRange() const { return m_attackRange; }
    int getDamage() const { return m_damage; }
    virtual char getSymbol() const;
    Lane getLane() const { return m_homeLane; }

    bool canAttackAir() const { return m_canAttackAir; }
    void setCanAttackAir(bool canAttack) { m_canAttackAir = canAttack; }

    /// Selects this entity's preferred target from the board, or nullptr when
    /// nothing valid is in play. Overridden to change targeting policy, e.g.
    /// TowerPrioritizingEntity ignores troops entirely.
    virtual std::shared_ptr<Entity> findTarget(const Board& board) const;

protected: // Changed from private to protected so child classes can access them.
    EntityType m_type;
    int m_x, m_y;
    float m_moveTimer;
    float m_moveSpeed;
    int m_health;
    bool m_isPlayer;
    int m_maxHealth;
    int m_attackRange;
    int m_damage;
    bool m_isFlying;
    bool m_canAttackAir;
    Lane m_homeLane;
    virtual void calculateStats() = 0;
    virtual void move(const Board& board) = 0;
    
    void logWarning(const std::string& message) const {
        std::cerr << "Warning [Entity " << getSymbol() << " at (" << m_x << "," << m_y << ")]: " << message << std::endl;
    }
};

}  // namespace cr
