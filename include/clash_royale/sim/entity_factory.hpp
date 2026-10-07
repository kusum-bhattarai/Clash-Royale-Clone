#pragma once

#include "clash_royale/core/types.hpp"
#include "clash_royale/sim/entity.hpp"
#include <memory>

namespace cr {

class EntityFactory {
public:
    static std::shared_ptr<Entity> create(EntityType type, int x, int y, bool isPlayer, Lane lane);
};

}  // namespace cr
