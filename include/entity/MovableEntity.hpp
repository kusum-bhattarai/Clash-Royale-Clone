#pragma once
#include "entity.hpp"

// This is an abstract class for any entity that can move.
class MovableEntity : public Entity {
public:
    using Entity::Entity;
    void move(const Board& board) override;
};