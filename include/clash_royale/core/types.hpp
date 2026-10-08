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
/// These are the dimensions of `Arena::standard()`. An Arena carries its own
/// extent, so a match may be played on a different size; these remain as the
/// default the shipped game uses.
/// Both are odd so that the playable interior, which runs from 1 to
/// `extent - 2`, has a true centre column and row. With an even width the
/// interior spanned 1..38, centred on 19.5, while the towers and bridges were
/// placed symmetrically about 20, so the whole formation sat half a tile right
/// of the frame it was drawn in.
inline constexpr int kArenaWidth = 41;
inline constexpr int kArenaHeight = 35;

/// The default simulation step, in seconds.
///
/// The original code hardcoded this 0.1 in three unrelated places: the entity
/// move timer, the elixir timer and the match clock. Every one of those now
/// reads the `dt` threaded through from Simulation::step().
inline constexpr float kDefaultTimeStep = 0.1f;

/// An integer tile coordinate.
struct Point {
    int x = 0;
    int y = 0;
};

inline bool operator==(Point a, Point b) {
    return a.x == b.x && a.y == b.y;
}

inline bool operator!=(Point a, Point b) {
    return !(a == b);
}

/// Whether a unit travels on the ground or through the air.
///
/// Lives here rather than beside the card types because the arena's passability
/// rules are expressed in terms of it, and core cannot depend on sim.
enum class MovementDomain {
    Ground,
    Air,
};

/// Which side of the arena a unit is deployed down.
enum class Lane {
    LEFT,
    RIGHT,
    CENTER,
};

}  // namespace cr
