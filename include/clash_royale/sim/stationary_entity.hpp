#pragma once
#include "clash_royale/core/types.hpp"
#include "clash_royale/sim/entity.hpp"

namespace cr {

// This class represents the entity that doesnt move
class StationaryEntity : public Entity {
public:
    using Entity::Entity;

    void move(const Board& /*board*/) override {
        // This function is empty cuz towers and canon dont move.
    }
};

}  // namespace cr
