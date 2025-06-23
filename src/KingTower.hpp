#pragma once
#include "StationaryEntity.hpp"

class KingTower : public StationaryEntity {
public:
    KingTower(EntityType type, int x, int y, bool isPlayer, int health, Lane lane);
    void calculateStats() override;
};