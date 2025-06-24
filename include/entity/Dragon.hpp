#pragma once
#include "MovableEntity.hpp"

class Dragon : public MovableEntity {
public:
    Dragon(EntityType type, int x, int y, bool isPlayer, int health, Lane lane);
    void calculateStats() override;
    void move(const Board& board) override;
    char getSymbol() const override { return m_isPlayer ? 'D' : 'd'; }
};