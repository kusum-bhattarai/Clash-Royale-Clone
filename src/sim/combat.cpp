#include "clash_royale/sim/combat.hpp"

#include <algorithm>

#include "clash_royale/sim/entity.hpp"

namespace cr {
namespace {

template <typename T>
bool containsOrEmpty(const std::vector<T>& values, const T& needle) {
    return values.empty() || std::find(values.begin(), values.end(), needle) != values.end();
}

/// A modifier applies when every condition it specifies matches. Conditions
/// left empty match anything, so a modifier with none always applies.
bool applies(const DamageModifier& modifier, const Entity& target) {
    const CardSpec& spec = target.spec();

    if (!containsOrEmpty(modifier.againstArmor, spec.armor)) {
        return false;
    }
    if (!containsOrEmpty(modifier.againstDomain, spec.domain)) {
        return false;
    }
    if (!containsOrEmpty(modifier.againstCards, spec.id)) {
        return false;
    }
    if (modifier.againstBuildings.has_value() && *modifier.againstBuildings != spec.isBuilding) {
        return false;
    }
    return true;
}

}  // namespace

bool isWithinRange(const Entity& attacker, const Entity& target, int range) {
    const int dx = attacker.getX() - target.getX();
    const int dy = attacker.getY() - target.getY();
    return (dx * dx + dy * dy) <= range * range;
}

int resolveDamage(const Entity& attacker, const Entity& target, const CombatRules& rules, Rng& rng) {
    // Matchup bonuses and armor scaling were two switch statements over a
    // closed EntityType enum, so a downstream card could not participate in
    // either. They are now data: the attacker carries its modifiers and the
    // target carries its incoming multiplier.
    float multiplier = 1.0f;
    for (const DamageModifier& modifier : attacker.spec().damageModifiers) {
        if (applies(modifier, target)) {
            multiplier *= modifier.multiplier;
        }
    }

    const float scaled =
        static_cast<float>(attacker.getDamage()) * multiplier * target.spec().incomingDamageMultiplier;
    int damage = static_cast<int>(scaled);

    // Exactly one critical roll, applied to final damage.
    if (rng.chance(rules.criticalChance)) {
        damage = static_cast<int>(static_cast<float>(damage) * rules.criticalMultiplier);
    }

    return std::max(1, damage);
}

}  // namespace cr
