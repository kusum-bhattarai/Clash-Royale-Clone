#include "clash_royale/sim/board.hpp"

#include <algorithm>
#include <climits>
#include <utility>

#include "clash_royale/sim/card_registry.hpp"
#include "clash_royale/sim/combat.hpp"

namespace cr {

bool BoardObstacles::blocks(Point p, MovementDomain domain) const {
    return m_board->occupancyAt(p.x, p.y, domain) > 0;
}

Board::Board(Arena arena) : m_arena(std::move(arena)) {
    const std::size_t cells =
        static_cast<std::size_t>(m_arena.width()) * static_cast<std::size_t>(m_arena.height());
    m_groundOccupancy.assign(cells, 0);
    m_airOccupancy.assign(cells, 0);
}

std::size_t Board::tileIndex(int x, int y) const {
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(m_arena.width()) + static_cast<std::size_t>(x);
}

int Board::occupancyAt(int x, int y, MovementDomain domain) const {
    if (!m_arena.inBounds(x, y)) {
        return 0;
    }
    const auto& counts = (domain == MovementDomain::Air) ? m_airOccupancy : m_groundOccupancy;
    return counts[tileIndex(x, y)];
}

void Board::rebuildOccupancy() {
    std::fill(m_groundOccupancy.begin(), m_groundOccupancy.end(), std::uint16_t{0});
    std::fill(m_airOccupancy.begin(), m_airOccupancy.end(), std::uint16_t{0});

    for (const auto& entity : entities) {
        if (!entity->isAlive() || !m_arena.inBounds(entity->getX(), entity->getY())) {
            continue;
        }
        auto& counts = entity->isFlying() ? m_airOccupancy : m_groundOccupancy;
        ++counts[tileIndex(entity->getX(), entity->getY())];
    }
}

std::shared_ptr<Entity> Board::spawn(const CardSpec& spec, int x, int y, bool isPlayer, Lane lane) {
    std::shared_ptr<Entity> entity = createEntity(spec, m_arena, x, y, isPlayer, lane);
    entities.push_back(entity);
    return entity;
}

void Board::addEntity(std::shared_ptr<Entity> entity) {
    entities.push_back(std::move(entity));
}

void Board::updateEntities(float dt) {
    rebuildOccupancy();

    for (auto& entity : entities) {
        if (!entity->isAlive()) {
            continue;
        }

        const int fromX = entity->getX();
        const int fromY = entity->getY();
        entity->update(*this, dt);

        // Occupancy is kept exact as the loop proceeds rather than only
        // snapshotted at the start of the tick. Without this, two units both
        // see a tile as free and both step into it within the same tick.
        if (entity->getX() != fromX || entity->getY() != fromY) {
            auto& counts = entity->isFlying() ? m_airOccupancy : m_groundOccupancy;
            if (m_arena.inBounds(fromX, fromY)) {
                std::uint16_t& previous = counts[tileIndex(fromX, fromY)];
                if (previous > 0) {
                    --previous;
                }
            }
            if (m_arena.inBounds(entity->getX(), entity->getY())) {
                ++counts[tileIndex(entity->getX(), entity->getY())];
            }
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
