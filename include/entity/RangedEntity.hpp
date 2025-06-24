#pragma once
#include "entity/MovableEntity.hpp"

// This is an abstract class for any movable entity that attacks from a distance
class RangedEntity : public MovableEntity {
public:
    using MovableEntity::MovableEntity;
    void move(const Board& board) override;
};