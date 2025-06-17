#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include <utility>
#include <iostream>

class Board;

enum class EntityType {
    KING_TOWER,
    QUEEN_TOWER,
    KNIGHT,
    GOLEM,
    PEKKA,
    GOBLINS,
    DRAGON,
    WIZARD,
    ARCHERS,
    CANON
};

class Entity {
public:
    Entity(EntityType type, int x, int y, bool isPlayer, int health);
    
    void update(const Board& board);
    void takeDamage(int damage);
    bool isAlive() const;
    
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

    void setIsFlying(bool flying) { m_isFlying = flying; }
    void setCanAttackAir(bool canAttack) { m_canAttackAir = canAttack; }

private:
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
    
    void move(const Board& board); // Moved back to private
    void calculateStats();
    std::pair<int, int> findTarget(const Board& board) const;
    bool isValidTarget(const Entity& target) const;
    
    void logWarning(const std::string& message) const {
        std::cerr << "Warning [Entity " << getSymbol() << " at (" << m_x << "," << m_y << ")]: " << message << std::endl;
    }
};