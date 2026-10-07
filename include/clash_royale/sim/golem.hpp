#pragma once
#include "clash_royale/core/types.hpp"
#include "clash_royale/sim/tower_prioritizing_entity.hpp"

namespace cr {

class Golem : public TowerPrioritizingEntity {
public:
    Golem(EntityType type, int x, int y, bool isPlayer, int health, Lane lane);
    void calculateStats() override;
    char getSymbol() const override { return m_isPlayer ? 'G' : 'g'; }
};

}  // namespace cr
