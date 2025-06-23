#pragma once
#include "RangedEntity.hpp"

class Wizard : public RangedEntity {
public:
    Wizard(EntityType type, int x, int y, bool isPlayer, int health, Lane lane);
    void calculateStats() override;
};