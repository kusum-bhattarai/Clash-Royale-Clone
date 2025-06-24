#pragma once

#include "entity/entity.hpp"
#include "core/game.hpp"
#include <memory>

class EntityFactory {
public:
    static std::shared_ptr<Entity> create(EntityType type, int x, int y, bool isPlayer, Lane lane);
};