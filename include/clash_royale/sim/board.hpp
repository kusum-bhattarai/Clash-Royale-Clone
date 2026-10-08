#pragma once
#include <cstdint>

#include "clash_royale/core/arena.hpp"
#include "clash_royale/path/navigator.hpp"
#include "clash_royale/core/types.hpp"
#include "clash_royale/sim/combat.hpp"
#include "clash_royale/sim/entity.hpp"
#include <vector>
#include <memory>

namespace cr {

class Board;

/// Exposes a board's unit occupancy to the pathfinder.
class BoardObstacles final : public Obstacles {
public:
    explicit BoardObstacles(const Board& board) : m_board(&board) {}
    bool blocks(Point p, MovementDomain domain) const override;

private:
    const Board* m_board;
};

class Board {
public:
    /// \param arena the field this board is played on. Entities move according
    ///        to its terrain, so it must outlive them -- the Board owns it.
    explicit Board(Arena arena = Arena::standard());

    const Arena& arena() const { return m_arena; }

    void addEntity(std::shared_ptr<Entity> entity);

    /// Creates an entity from a card on this board's arena and adds it.
    ///
    /// Preferred over building one separately, because it cannot disagree with
    /// the board about which arena the entity was placed on.
    std::shared_ptr<Entity> spawn(const CardSpec& spec, int x, int y, bool isPlayer, Lane lane);
    void updateEntities(float dt = kDefaultTimeStep);
    /// Resolves one round of attacks. Randomness is supplied by the caller so
    /// that a match is reproducible from its seed.
    void handleCombat(Rng& rng, float dt = kDefaultTimeStep);

    /// Living entities of `domain` standing on this tile.
    ///
    /// Snapshotted at the start of updateEntities() and then kept exact as each
    /// entity moves, so two units cannot both see one tile as free and step into
    /// it on the same tick.
    int occupancyAt(int x, int y, MovementDomain domain) const;

    /// A view of this board's occupancy, usable as path obstacles.
    ///
    /// Returned by value so it cannot outlive or be stranded by a copy of the
    /// board; it borrows the board, so keep it only for the current call.
    BoardObstacles obstacles() const { return BoardObstacles{*this}; }

    /// Shared routing for the entities on this board.
    ///
    /// Non-const through a const Board because it is a cache: flow fields are
    /// shared derived state, and asking for a route is logically a read.
    Navigator& navigator() const { return m_navigator; }

    void setCombatRules(const CombatRules& rules) { m_combatRules = rules; }
    const CombatRules& combatRules() const { return m_combatRules; }
    std::vector<std::shared_ptr<Entity>>& getEntities();
    const std::vector<std::shared_ptr<Entity>>& getEntities() const;   
    const std::vector<std::shared_ptr<Entity>>& getAllEntities() const {
        return entities;
    }

private:
    void rebuildOccupancy();
    std::size_t tileIndex(int x, int y) const;

    Arena m_arena;
    std::vector<std::shared_ptr<Entity>> entities;
    CombatRules m_combatRules;

    // Counts rather than single occupants: several units may share a tile
    // transiently, and a count stays correct when they do.
    std::vector<std::uint16_t> m_groundOccupancy;
    std::vector<std::uint16_t> m_airOccupancy;

    mutable Navigator m_navigator;
};

}  // namespace cr
