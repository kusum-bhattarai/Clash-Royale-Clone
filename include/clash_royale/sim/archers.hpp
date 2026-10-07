#pragma once
#include "clash_royale/core/types.hpp"
#include "clash_royale/sim/ranged_entity.hpp"

namespace cr {

class Archers : public RangedEntity {
public:
    Archers(EntityType type, int x, int y, bool isPlayer, int health, Lane lane);
    void calculateStats() override;
    char getSymbol() const override { return m_isPlayer ? 'A' : 'a'; }
};

}  // namespace cr
