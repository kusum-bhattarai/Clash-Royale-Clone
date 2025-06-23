#pragma once
#include "StationaryEntity.hpp"

class Canon : public StationaryEntity {
public:
    Canon(EntityType type, int x, int y, bool isPlayer, int health, Lane lane);
    void calculateStats() override;
};