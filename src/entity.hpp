#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include <utility>

// Forward declaration
class Board;

enum class EntityType {
    KING_TOWER,
    QUEEN_TOWER,
    KNIGHT,       // Speed: 1.0, Medium armor
    GOLEM,        // Speed: 0.5, Heavy armor
    PEKKA,        // Speed: 0.75, Heavy armor
    GOBLINS,      // Speed: 1.2, Light armor
    DRAGON,       // Speed: 1.5, Flying unit
    WIZARD,       // Speed: 1.0, Ranged attacker
    ARCHERS,      // Speed: 1.2, Ranged attacker
    CANON         // Speed: 0, Stationary defense
};

class Entity {
public:
    Entity(EntityType type, int x, int y, bool isPlayer, int health);
    
    // Public interface
    void update();
    void takeDamage(int damage);
    bool isAlive() const;
    
    // Getters
    EntityType getType() const { return m_type; }
    int getX() const { return m_x; }
    int getY() const { return m_y; }
    int getHealth() const { return m_health; }
    bool getIsPlayer() const { return m_isPlayer; }
    bool isFlying() const { return m_isFlying; }
    bool canAttackAir() const { return m_canAttackAir; }
    int getAttackRange() const { return m_attackRange; }
    int getDamage() const { return m_damage; }
    char getSymbol() const;

    // Add setters
    void setIsFlying(bool flying) { m_isFlying = flying; }
    void setCanAttackAir(bool canAttack) { m_canAttackAir = canAttack; }

private:
    // Private member variables
    EntityType m_type;
    int m_x, m_y;
    float m_moveTimer;
    float m_moveSpeed;
    int m_health;
    bool m_isPlayer;
    const int m_maxHealth;
    int m_attackRange;
    int m_damage;
    bool m_isFlying;
    bool m_canAttackAir;
    
    // Private helper functions
    void move();
    void calculateStats();
    std::pair<int, int> findTarget(const Board& board) const;
    bool isValidTarget(const Entity& target) const;
};
