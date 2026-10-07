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
    // their health; only troops are reaped.
    entities.erase(std::remove_if(entities.begin(), entities.end(),
                                  [](const std::shared_ptr<Entity>& e) {
                                      return !e->isAlive() &&
                                             e->getType() != EntityType::KING_TOWER &&
                                             e->getType() != EntityType::QUEEN_TOWER;
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

        // Pick the nearest valid enemy within reach. Note this duplicates the
        // targeting policy in Entity::findTarget rather than reusing it, which
        // is why the Golem's building preference has to be special-cased here
        // as well. Unifying the two behind the card registry's target
        // preference is a later change.
        std::shared_ptr<Entity> bestTarget = nullptr;
        int minDistance = INT_MAX;

        for (auto& target : entities) {
            if (!target->isAlive() || target->getIsPlayer() == attacker->getIsPlayer()) {
                continue;
            }
            if (target->isFlying() && !attacker->canAttackAir()) {
                continue;
            }
            if (!isWithinRange(*attacker, *target, attacker->getAttackRange())) {
                continue;
            }

            if (attacker->getType() == EntityType::GOLEM &&
                target->getType() != EntityType::KING_TOWER &&
                target->getType() != EntityType::QUEEN_TOWER &&
                target->getType() != EntityType::CANON) {
                continue;
            }

            const int distance = std::abs(attacker->getX() - target->getX()) +
                                 std::abs(attacker->getY() - target->getY());
            if (distance < minDistance) {
                minDistance = distance;
                bestTarget = target;
            }
        }

        if (bestTarget) {
            bestTarget->takeDamage(resolveDamage(*attacker, *bestTarget, m_combatRules, rng));
            attacker->registerAttack();
        }
    }
}

const std::vector<std::shared_ptr<Entity>>& Board::getEntities() const {
    return entities;
}

std::vector<std::shared_ptr<Entity>>& Board::getEntities() {
    return entities;
}

}  // namespace cr
