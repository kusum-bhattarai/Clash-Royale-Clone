#pragma once
#include "clash_royale/core/types.hpp"
#include "clash_royale/sim/stationary_entity.hpp"

namespace cr {

class Canon : public StationaryEntity {
public:
    Canon(EntityType type, int x, int y, bool isPlayer, int health, Lane lane);
    void calculateStats() override;
    char getSymbol() const override { return m_isPlayer ? 'C' : 'c'; }
};

}  // namespace cr
