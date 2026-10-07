#pragma once
#include "clash_royale/core/types.hpp"
#include "clash_royale/sim/movable_entity.hpp"

namespace cr {

// This is an abstract class for any movable entity that attacks from a distance
class RangedEntity : public MovableEntity {
public:
    using MovableEntity::MovableEntity;
    void move(const Board& board) override;
};

}  // namespace cr
