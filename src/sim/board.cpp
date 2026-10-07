#include "clash_royale/sim/board.hpp"

#include <algorithm>
#include <climits>

#include "clash_royale/sim/combat.hpp"

namespace cr {

void Board::addEntity(std::shared_ptr<Entity> entity) {
    entities.push_back(std::move(entity));
}

void Board::updateEntities(float dt) {
    for (auto& entity : entities) {
        if (entity->isAlive()) {
            entity->update(*this, dt);
        }
    }

    // Dead towers are deliberately retained so win conditions can still read
    // their health; only troops and ordinary buildings are reaped.
    entities.erase(std::remove_if(entities.begin(), entities.end(),
                                  [](const std::shared_ptr<Entity>& e) {
                                      return !e->isAlive() && e->spec().towerRole == TowerRole::None;
                                  }),
                   entities.end());
}

void Board::handleCombat(Rng& rng, float dt) {
    for (auto& attacker : entities) {
        if (!attacker->isAlive()) {
            continue;
        }

        // Cooldowns age for every living entity, in or out of combat.
        attacker->tickAttackCooldown(dt);
        if (!attacker->canAttack()) {
            continue;
        }

        // Targeting is delegated to the entity rather than reimplemented here.
        // This loop used to carry its own copy of the policy -- including a
        // hardcoded special case for the Golem's building preference -- which
        // could drift from Entity::findTarget. Since findTarget returns the
        // *nearest* admissible enemy, no closer in-range target can exist, so
        // a single range check is sufficient.
        const std::shared_ptr<Entity> target = attacker->findTarget(*this);
        if (!target || !isWithinRange(*attacker, *target, attacker->getAttackRange())) {
            continue;
        }

        target->takeDamage(resolveDamage(*attacker, *target, m_combatRules, rng));
        attacker->registerAttack();
    }
}

const std::vector<std::shared_ptr<Entity>>& Board::getEntities() const {
    return entities;
}

std::vector<std::shared_ptr<Entity>>& Board::getEntities() {
    return entities;
}

}  // namespace cr
