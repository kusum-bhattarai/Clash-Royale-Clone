#pragma once
#include "clash_royale/core/types.hpp"
#include "clash_royale/sim/movable_entity.hpp"

namespace cr {

// This class is for units that ignore other troops and only attack towers.
class TowerPrioritizingEntity : public MovableEntity {
public:
    using MovableEntity::MovableEntity;
    std::shared_ptr<Entity> findTarget(const Board& board) const override;
};

}  // namespace cr
