#pragma once
#include "clash_royale/core/types.hpp"
#include "clash_royale/sim/movable_entity.hpp"

namespace cr {

class Pekka : public MovableEntity {
public:
    Pekka(EntityType type, int x, int y, bool isPlayer, int health, Lane lane);
    void calculateStats() override;
    char getSymbol() const override { return m_isPlayer ? 'P' : 'p'; }
};

}  // namespace cr
