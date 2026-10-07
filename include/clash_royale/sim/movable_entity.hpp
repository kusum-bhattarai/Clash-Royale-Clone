#pragma once
#include "clash_royale/core/types.hpp"
#include "clash_royale/sim/entity.hpp"

namespace cr {

// This is an abstract class for any entity that can move.
class MovableEntity : public Entity {
public:
    using Entity::Entity;
    void move(const Board& board) override;
};

}  // namespace cr
