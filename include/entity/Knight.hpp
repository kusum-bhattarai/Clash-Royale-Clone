#pragma once
#include "MovableEntity.hpp"

class Knight : public MovableEntity {
public:
    Knight(EntityType type, int x, int y, bool isPlayer, int health, Lane lane);
    void calculateStats() override;
    char getSymbol() const override { return m_isPlayer ? 'K' : 'k'; }
};