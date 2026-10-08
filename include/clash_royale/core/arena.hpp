#pragma once

#include <cstdint>
#include <vector>

#include "clash_royale/core/types.hpp"

namespace cr {

/// What occupies a tile of the arena.
enum class Tile : std::uint8_t {
    Ground,   ///< open, walkable by anything
    Water,    ///< ground units cannot enter; air crosses freely
    Bridge,   ///< a crossing laid over water, walkable by anything
    Blocked,  ///< impassable to everything, including air
};

/// The playing field: its extent and its terrain.
///
/// Arena takes over geometry from the `kArenaWidth` / `kArenaHeight` constants,
/// which the simulation previously read off the *renderer* -- entities included
/// a presentation header purely to clamp their coordinates. Dimensions are now
/// per-arena rather than compile-time, so a match can be played on a different
/// board.
///
/// Terrain is what gives `Lane` meaning. Before this, a lane was recorded on
/// every entity and read by nothing: the field was open, so units simply walked
/// at whatever was nearest. With a river splitting the arena, a ground unit has
/// to reach a bridge, and which bridge is its lane.
class Arena {
public:
    /// An open arena of the given size, with a one-tile border of Blocked
    /// tiles standing in for the frame the front-end draws.
    Arena(int width, int height);

    /// The arena the game ships with: 40x35, with a river along the midline
    /// and a three-tile bridge in each lane.
    static Arena standard();

    int width() const { return m_width; }
    int height() const { return m_height; }

    bool inBounds(int x, int y) const;

    /// The interior is `[1, width - 2]` by `[1, height - 2]`, excluding the
    /// border. Entities are kept inside it.
    bool inInterior(int x, int y) const;

    /// The tile at (x, y), or Blocked when out of bounds.
    Tile tile(int x, int y) const;
    void setTile(int x, int y, Tile tile);

    /// True when a unit of this domain may occupy (x, y).
    ///
    /// Ground units are stopped by Water and Blocked; air units only by
    /// Blocked. Everything outside the interior is impassable.
    bool isPassable(int x, int y, MovementDomain domain) const;

    /// Clamps a position into the interior.
    void clampToInterior(int& x, int& y) const;

    /// The rows the river occupies, ascending. Empty when there is none.
    const std::vector<int>& riverRows() const { return m_riverRows; }
    bool hasRiver() const { return !m_riverRows.empty(); }

    /// Which side of the river a row lies on: -1 above, +1 below, 0 on it.
    ///
    /// Meaningless without a river, so check hasRiver() first; it returns 0 for
    /// every row when there is none.
    int riverSide(int y) const;

    /// Centre columns of the crossings, ascending. Empty when there is none.
    const std::vector<int>& bridgeColumns() const { return m_bridgeColumns; }

    /// The crossing a unit should head for.
    ///
    /// LEFT takes the leftmost bridge and RIGHT the rightmost. CENTER, which
    /// no built-in spawn uses, takes whichever is nearer to `fromX`. Returns
    /// `fromX` unchanged when the arena has no bridges.
    int bridgeColumnFor(Lane lane, int fromX) const;

    /// Cuts a river along `rows`, then lays a bridge of `bridgeWidth` tiles
    /// centred on each column in `centres`.
    ///
    /// Replaces any previous river.
    void carveRiver(std::vector<int> rows, std::vector<int> centres, int bridgeWidth);

private:
    std::size_t indexOf(int x, int y) const;

    int m_width;
    int m_height;
    std::vector<Tile> m_tiles;
    std::vector<int> m_riverRows;
    std::vector<int> m_bridgeColumns;
};

}  // namespace cr
