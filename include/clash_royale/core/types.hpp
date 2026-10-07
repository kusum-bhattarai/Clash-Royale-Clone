#pragma once

/// \file
/// Fundamental value types shared by every layer of the library.
///
/// This header exists to break two dependency inversions that the original
/// layout carried: entities reached into the *renderer* for arena dimensions,
/// and the entity factory included the whole `Game` header just to see `Lane`.
/// Both of those pulled presentation and terminal I/O into the simulation. The
/// shared vocabulary now lives here, below everything else, and depends on
/// nothing.

namespace cr {

/// Arena extent in tiles.
///
/// Entities are clamped to the interior, i.e. `[1, kArenaWidth - 2]` by
/// `[1, kArenaHeight - 2]`, leaving a one-tile border for the frame drawn by
/// the terminal front-end.
///
/// These are owned by the simulation rather than by any renderer. A later
/// change replaces them with a configurable `Arena` that also carries terrain;
/// until then they remain compile-time constants so existing behavior is
/// preserved exactly.
inline constexpr int kArenaWidth = 40;
inline constexpr int kArenaHeight = 35;

/// The default simulation step, in seconds.
///
/// The original code hardcoded this 0.1 in three unrelated places: the entity
/// move timer, the elixir timer and the match clock. Every one of those now
/// reads the `dt` threaded through from Simulation::step().
inline constexpr float kDefaultTimeStep = 0.1f;

/// Which side of the arena a unit is deployed down.
enum class Lane {
    LEFT,
    RIGHT,
    CENTER,
};

/// Identifies a kind of troop or building.
///
/// This fixed enumeration is why adding a unit currently means editing the
/// factory and both damage-modifier switches. A later change replaces it with a
/// data-driven card registry so downstream users can define units without
/// modifying the library.
enum class EntityType {
    KING_TOWER,
    QUEEN_TOWER,
    KNIGHT,
    GOLEM,
    PEKKA,
    GOBLINS,
    DRAGON,
    WIZARD,
    ARCHERS,
    CANON,
};

}  // namespace cr
