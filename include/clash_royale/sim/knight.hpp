#pragma once
#include "clash_royale/core/types.hpp"
#include "clash_royale/sim/movable_entity.hpp"

namespace cr {

class Knight : public MovableEntity {
public:
    Knight(EntityType type, int x, int y, bool isPlayer, int health, Lane lane);
    void calculateStats() override;
    char getSymbol() const override { return m_isPlayer ? 'K' : 'k'; }
};

}  // namespace cr
