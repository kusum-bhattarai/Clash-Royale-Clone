#pragma once
#include "clash_royale/core/types.hpp"
#include "clash_royale/sim/combat.hpp"
#include "clash_royale/sim/entity.hpp"
#include <vector>
#include <memory>

namespace cr {

class Board {
public:
    void addEntity(std::shared_ptr<Entity> entity);
    void updateEntities(float dt = kDefaultTimeStep);
    /// Resolves one round of attacks. Randomness is supplied by the caller so
    /// that a match is reproducible from its seed.
    void handleCombat(Rng& rng);

    void setCombatRules(const CombatRules& rules) { m_combatRules = rules; }
    const CombatRules& combatRules() const { return m_combatRules; }
    std::vector<std::shared_ptr<Entity>>& getEntities();
    const std::vector<std::shared_ptr<Entity>>& getEntities() const;   
    const std::vector<std::shared_ptr<Entity>>& getAllEntities() const {
        return entities;
    }

private:
    std::vector<std::shared_ptr<Entity>> entities;
    CombatRules m_combatRules;
};

}  // namespace cr
