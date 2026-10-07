#pragma once

#include "clash_royale/core/rng.hpp"

namespace cr {

class Entity;

/// Tunable parameters for damage resolution.
///
/// Exposed as data so downstream users can rebalance combat, and so tests can
/// switch the critical-hit roll fully off and assert exact damage values.
struct CombatRules {
    /// Probability that an attack critically hits. Set to 0 for deterministic
    /// damage, or 1 to force every attack to crit.
    float criticalChance = 0.05f;

    /// Multiplier applied to final damage on a critical hit.
    float criticalMultiplier = 1.5f;
};

/// True when `target` lies within `range` tiles of `attacker`, measured as
/// squared Euclidean distance.
bool isWithinRange(const Entity& attacker, const Entity& target, int range);

/// Computes the damage `attacker` deals to `target` for a single hit.
///
/// Order of operations: base damage, then the attacker/target type matchup
/// multipliers, then the target's armor class, then at most one critical-hit
/// roll. Damage is floored at 1 so an attack is never fully absorbed.
///
/// The matchup and armor tables switch on EntityType, which is why adding a
/// unit currently means editing this file. A later change moves these
/// modifiers into the card registry as data.
int resolveDamage(const Entity& attacker, const Entity& target, const CombatRules& rules, Rng& rng);

}  // namespace cr
