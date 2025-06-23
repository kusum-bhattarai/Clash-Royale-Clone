#pragma once
#include "MovableEntity.hpp"

class Goblins : public MovableEntity {
public:
    Goblins(EntityType type, int x, int y, bool isPlayer, int health, Lane lane);

    void calculateStats() override;
    void move(const Board& board) override;
};