#pragma once
#include "StationaryEntity.hpp"

class QueenTower : public StationaryEntity {
public:
    QueenTower(EntityType type, int x, int y, bool isPlayer, int health, Lane lane);
    void calculateStats() override;
};