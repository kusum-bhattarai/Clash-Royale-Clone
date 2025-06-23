#pragma once
#include "RangedEntity.hpp"

class Archers : public RangedEntity {
public:
    Archers(EntityType type, int x, int y, bool isPlayer, int health, Lane lane);
    void calculateStats() override;
};