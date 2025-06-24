#pragma once
#include "TowerPrioritizingEntity.hpp"

class Golem : public TowerPrioritizingEntity {
public:
    Golem(EntityType type, int x, int y, bool isPlayer, int health, Lane lane);
    void calculateStats() override;
    char getSymbol() const override { return m_isPlayer ? 'G' : 'g'; }
};