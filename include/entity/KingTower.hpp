#pragma once
#include "entity/StationaryEntity.hpp"

class KingTower : public StationaryEntity {
public:
    KingTower(EntityType type, int x, int y, bool isPlayer, int health, Lane lane);
    void calculateStats() override;
    char getSymbol() const override { return m_isPlayer ? 'T' : 't'; }
};