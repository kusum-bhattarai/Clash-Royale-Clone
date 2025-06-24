#pragma once
#include "entity/MovableEntity.hpp"

// This class is for units that ignore other troops and only attack towers.
class TowerPrioritizingEntity : public MovableEntity {
public:
    using MovableEntity::MovableEntity;
    std::shared_ptr<Entity> findTarget(const Board& board) const override;
};