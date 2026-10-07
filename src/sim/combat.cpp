#include "clash_royale/sim/combat.hpp"

#include <algorithm>

#include "clash_royale/sim/entity.hpp"

namespace cr {
namespace {

/// Matchup bonuses keyed on who is attacking what.
float matchupMultiplier(const Entity& attacker, const Entity& target) {
    float multiplier = 1.0f;

    switch (target.getType()) {
        case EntityType::KING_TOWER:
        case EntityType::QUEEN_TOWER:
            if (attacker.getType() == EntityType::GOLEM) {
                multiplier *= 1.5f;  // Golem hits buildings harder
            }
            break;

        case EntityType::CANON:
            if (attacker.getType() == EntityType::PEKKA) {
                multiplier *= 1.3f;  // PEKKA hits buildings harder
            }
            break;

        default:
            break;
    }

    switch (attacker.getType()) {
        case EntityType::WIZARD:
            if (target.isFlying()) {
                multiplier *= 1.2f;  // splash is effective against air
            }
            break;

        case EntityType::PEKKA:
            multiplier *= 1.2f;  // heavy armor penetration
            break;

        case EntityType::GOBLINS:
            if (target.getType() == EntityType::KNIGHT ||
                target.getType() == EntityType::WIZARD ||
                target.getType() == EntityType::ARCHERS) {
                multiplier *= 1.1f;  // effective against light armor
            }
            break;

        default:
            break;
    }

    return multiplier;
}

/// Incoming-damage scaling from the target's armor class.
float armorMultiplier(const Entity& target) {
    switch (target.getType()) {
        case EntityType::KNIGHT:
            return 0.8f;  // medium armor

        case EntityType::GOLEM:
        case EntityType::PEKKA:
            return 0.6f;  // heavy armor

        case EntityType::GOBLINS:
        case EntityType::WIZARD:
        case EntityType::ARCHERS:
            return 1.0f;  // light armor

        default:
            return 1.0f;
    }
}

}  // namespace

bool isWithinRange(const Entity& attacker, const Entity& target, int range) {
    const int dx = attacker.getX() - target.getX();
    const int dy = attacker.getY() - target.getY();
    return (dx * dx + dy * dy) <= range * range;
}

int resolveDamage(const Entity& attacker, const Entity& target, const CombatRules& rules, Rng& rng) {
    const float scaled = static_cast<float>(attacker.getDamage()) * matchupMultiplier(attacker, target) *
                         armorMultiplier(target);

    int damage = static_cast<int>(scaled);

    // Exactly one critical roll. This previously happened twice -- once before
    // armor scaling and once after -- which made 2.25x damage reachable and
    // lifted the effective crit rate from the intended 5% to about 9.75%.
    if (rng.chance(rules.criticalChance)) {
        damage = static_cast<int>(static_cast<float>(damage) * rules.criticalMultiplier);
    }

    return std::max(1, damage);
}

}  // namespace cr
