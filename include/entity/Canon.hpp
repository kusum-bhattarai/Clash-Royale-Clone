#pragma once
#include "entity/StationaryEntity.hpp"

class Canon : public StationaryEntity {
public:
    Canon(EntityType type, int x, int y, bool isPlayer, int health, Lane lane);
    void calculateStats() override;
    char getSymbol() const override { return m_isPlayer ? 'C' : 'c'; }
};