#pragma once

#include <vector>

#include "clash_royale/core/rng.hpp"
#include "clash_royale/sim/card.hpp"

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

/// The combined damage multiplier `attacker` enjoys against `target`, from the
/// attacker's modifiers alone -- armor scaling is the target's own
/// `incomingDamageMultiplier` and is applied separately.
///
/// Exposed because an AI weighing a counter needs the same answer combat does,
/// and reimplementing the matching rules would let the two drift apart.
float matchupMultiplier(const CardSpec& attacker, const CardSpec& target);

/// True when `target` lies within `range` tiles of `attacker`, measured as
/// squared Euclidean distance.
bool isWithinRange(const Entity& attacker, const Entity& target, int range);

/// Computes the damage `attacker` deals to `target` for a single hit.
///
/// Order of operations: base damage, then the attacker/target type matchup
/// multipliers, then the target's armor class, then at most one critical-hit
/// roll. Damage is floored at 1 so an attack is never fully absorbed.
///
/// Matchup bonuses come from the attacker's CardSpec::damageModifiers and armor
/// scaling from the target's CardSpec::incomingDamageMultiplier, so a card
/// defined outside the library participates in both without editing this file.
int resolveDamage(const Entity& attacker, const Entity& target, const CombatRules& rules, Rng& rng);

}  // namespace cr
