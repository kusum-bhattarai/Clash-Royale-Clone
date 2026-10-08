#pragma once

#include <optional>
#include <vector>

#include "clash_royale/core/types.hpp"

namespace cr {

class Arena;

/// Dynamic blockers layered on top of the arena's static terrain.
///
/// Terrain belongs to the Arena and changes rarely; this is for what moves,
/// principally other units. Keeping the two apart means a path request can
/// consider congestion without the arena having to know about entities.
class Obstacles {
public:
    virtual ~Obstacles() = default;

    /// True when `p` is blocked for a mover of this domain.
    ///
    /// Ground and air are asked separately so that a flier and a ground troop
    /// do not block each other.
    virtual bool blocks(Point p, MovementDomain domain) const = 0;
};

/// The tiles to walk, in order. Excludes the starting tile and ends at the goal.
using Path = std::vector<Point>;

struct PathRequest {
    Point from{};
    Point to{};
    MovementDomain domain = MovementDomain::Ground;

    /// Optional dynamic blockers.
    ///
    /// `to` is never treated as blocked, so a unit can always route to the tile
    /// its target is standing on; otherwise every melee unit would be unable to
    /// reach anything.
    const Obstacles* obstacles = nullptr;
};

/// Computes routes across an arena.
///
/// Replaces the greedy "step toward the target, and detour to your lane's
/// bridge if you must cross" rule, which only understood one river. An
/// implementation of this handles whatever terrain and congestion it is given,
/// including obstacles a downstream project introduces.
class Pathfinder {
public:
    virtual ~Pathfinder() = default;

    /// The route from `request.from` to `request.to`, or nullopt when none
    /// exists.
    ///
    /// Non-const because implementations reuse scratch buffers across calls --
    /// that reuse is most of why repeated queries are cheap. A Pathfinder is
    /// therefore not thread-safe; give each thread its own.
    virtual std::optional<Path> findPath(const Arena& arena, const PathRequest& request) = 0;
};

/// Cost of a straight step and of a diagonal one.
///
/// A diagonal is sqrt(2) rather than 1, so that diagonal travel is not
/// artificially cheap and paths do not develop a staircase bias.
inline constexpr float kStraightCost = 1.0f;
inline constexpr float kDiagonalCost = 1.41421356f;

}  // namespace cr
