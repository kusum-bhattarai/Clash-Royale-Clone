#pragma once
#include "MovableEntity.hpp"

class Pekka : public MovableEntity {
public:
    Pekka(EntityType type, int x, int y, bool isPlayer, int health, Lane lane);
    void calculateStats() override;
};