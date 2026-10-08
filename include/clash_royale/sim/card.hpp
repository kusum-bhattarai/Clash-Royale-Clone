#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "clash_royale/core/types.hpp"

namespace cr {

class Arena;
class Board;
class Entity;

/// How a unit steps toward its target.
///
/// This stays an enumeration of built-in styles rather than a polymorphic
/// strategy because movement is about to be rewritten around pathfinding; a
/// strategy interface designed now would be replaced immediately. Units needing
/// genuinely custom movement can override Entity::move via CardSpec::factory.
enum class MovementStyle {
    Stationary,    ///< buildings: never move
    AxisStep,      ///< close on the dominant axis, one tile per step
    Diagonal,      ///< close on both axes at once
    Zigzag,        ///< alternate axes between steps
    HoldAtRange,   ///< approach, then stop once the target is within reach
};

/// Broad damage-resistance category.
enum class ArmorClass {
    Light,
    Medium,
    Heavy,
    Building,
};

/// A unit's role in the win condition, if any.
enum class TowerRole {
    None,   ///< a troop or a non-scoring building
    Queen,  ///< a side tower: counts toward tower health
    King,   ///< destroying one ends the match
};

/// Which enemies a unit is willing to attack.
///
/// This replaces both the `canAttackAir` flag and the TowerPrioritizingEntity
/// subclass, and is consulted by Entity::findTarget and Board::handleCombat
/// alike -- previously those two carried independent copies of the policy, so
/// the Golem's building preference had to be special-cased in each.
struct TargetFilter {
    bool ground = true;
    bool air = false;

    /// When set, only buildings are considered. A unit with this set walks past
    /// troops without engaging them.
    bool buildingsOnly = false;
};

/// A conditional damage multiplier.
///
/// Every populated condition must match for the multiplier to apply; an empty
/// condition matches anything. A modifier with no conditions always applies.
struct DamageModifier {
    float multiplier = 1.0f;

    std::vector<ArmorClass> againstArmor;
    std::vector<MovementDomain> againstDomain;
    std::vector<std::string> againstCards;
    std::optional<bool> againstBuildings;
};

/// Everything that defines a troop or building.
///
/// This is the extension point of the library. Adding a unit used to mean
/// editing five files -- a new header, a new source file, the EntityType enum,
/// the factory switch, and both damage-modifier switches in combat. A unit is
/// now a value that can be registered from outside the library, and the
/// matchup tables that used to switch on a closed enum are data carried here.
struct CardSpec {
    /// Stable identifier, e.g. "knight". Used for lookup and serialization.
    std::string id;
    std::string displayName;

    /// Uppercase glyph for the owning player; the opponent's is lowercased.
    char symbol = '?';

    int health = 1;
    int damage = 0;
    int attackRange = 1;

    float moveSpeed = 0.0f;    ///< tiles per second; 0 means stationary
    float attackSpeed = 1.0f;  ///< attacks per second
    float elixirCost = 0.0f;

    /// False for units that exist on the board but cannot be deployed, such as
    /// the towers placed at the start of a match.
    bool deployable = true;

    /// Buildings do not move and are what `TargetFilter::buildingsOnly` seeks.
    bool isBuilding = false;

    TowerRole towerRole = TowerRole::None;

    MovementDomain domain = MovementDomain::Ground;
    MovementStyle movement = MovementStyle::AxisStep;
    ArmorClass armor = ArmorClass::Light;

    /// Scales all incoming damage. Light armor is 1.0, medium 0.8, heavy 0.6.
    float incomingDamageMultiplier = 1.0f;

    TargetFilter targets{};

    /// Outgoing damage bonuses this unit enjoys against particular targets.
    std::vector<DamageModifier> damageModifiers;

    /// Tiles toward midfield this unit is placed, relative to the lane spawn
    /// line. The Canon uses 2, which puts it slightly ahead of where troops
    /// appear rather than behind them.
    int spawnSetback = 0;

    /// Optional hook for units whose behavior cannot be expressed as data.
    ///
    /// When null, a plain Entity driven by this spec is created. Set it to
    /// return your own Entity subclass to override `move()`, `findTarget()` or
    /// `update()` while still describing stats declaratively.
    std::function<std::shared_ptr<Entity>(const CardSpec&, const Arena&, int x, int y, bool isPlayer, Lane lane)>
        factory;
};

}  // namespace cr
