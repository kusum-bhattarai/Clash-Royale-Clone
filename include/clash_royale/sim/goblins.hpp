#pragma once
#include "clash_royale/core/types.hpp"
#include "clash_royale/sim/movable_entity.hpp"

namespace cr {

class Goblins : public MovableEntity {
public:
    Goblins(EntityType type, int x, int y, bool isPlayer, int health, Lane lane);

    void calculateStats() override;
    void move(const Board& board) override;
    char getSymbol() const override { return m_isPlayer ? 'B' : 'b'; }
};

}  // namespace cr
