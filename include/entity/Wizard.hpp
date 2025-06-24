#pragma once
#include "entity/RangedEntity.hpp"

class Wizard : public RangedEntity {
public:
    Wizard(EntityType type, int x, int y, bool isPlayer, int health, Lane lane);
    void calculateStats() override;
    char getSymbol() const override { return m_isPlayer ? 'W' : 'w'; }
};