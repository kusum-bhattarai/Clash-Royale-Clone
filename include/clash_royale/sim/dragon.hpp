#pragma once
#include "clash_royale/core/types.hpp"
#include "clash_royale/sim/movable_entity.hpp"

namespace cr {

class Dragon : public MovableEntity {
public:
    Dragon(EntityType type, int x, int y, bool isPlayer, int health, Lane lane);
    void calculateStats() override;
    void move(const Board& board) override;
    char getSymbol() const override { return m_isPlayer ? 'D' : 'd'; }
};

}  // namespace cr
