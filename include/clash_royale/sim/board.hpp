#pragma once
#include "clash_royale/core/arena.hpp"
#include "clash_royale/core/types.hpp"
#include "clash_royale/sim/combat.hpp"
#include "clash_royale/sim/entity.hpp"
#include <vector>
#include <memory>

namespace cr {

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

    void setCombatRules(const CombatRules& rules) { m_combatRules = rules; }
    const CombatRules& combatRules() const { return m_combatRules; }
    std::vector<std::shared_ptr<Entity>>& getEntities();
    const std::vector<std::shared_ptr<Entity>>& getEntities() const;   
    const std::vector<std::shared_ptr<Entity>>& getAllEntities() const {
        return entities;
    }

private:
    Arena m_arena;
    std::vector<std::shared_ptr<Entity>> entities;
    CombatRules m_combatRules;
};

}  // namespace cr
