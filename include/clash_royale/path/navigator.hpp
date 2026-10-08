#pragma once

#include <cstddef>

#include "clash_royale/path/astar.hpp"
#include "clash_royale/path/direct.hpp"
#include "clash_royale/path/flow_field.hpp"

namespace cr {

/// Routes movement requests, choosing a strategy and sharing work between units.
///
/// The division of labour is the one RTS games settle on: a global route over
/// *static* terrain, which many units can share, plus local avoidance for the
/// things that move.
///
///   - Air requests take the straight line, falling back to a search when
///     something actually blocks it. Fliers cross water freely, so searching for
///     them is usually wasted work -- the line is 31x cheaper.
///   - Ground requests with no obstacle set take the shared flow field for that
///     goal. Deliberately terrain-only: a field that accounted for unit
///     positions would be stale the moment anything moved, and rebuilding it
///     every tick would throw away the sharing that makes it worth having.
///   - A request that does name obstacles gets A*, which can route around them.
///     This is the escape hatch for a unit that local avoidance has left stuck.
class Navigator {
public:
    std::optional<Path> route(const Arena& arena, const PathRequest& request);

    /// Discards cached flow fields. Needed only when the terrain changes, since
    /// fields never account for moving units in the first place.
    void invalidate();

    struct Stats {
        std::size_t requests = 0;
        std::size_t directLines = 0;
        std::size_t fieldLookups = 0;
        std::size_t searches = 0;
    };

    const Stats& stats() const { return m_stats; }
    std::size_t fieldsBuilt() const { return m_fields.fieldsBuilt(); }

private:
    DirectPathfinder m_direct;
    AStarPathfinder m_astar;
    FlowFieldPathfinder m_fields;
    Stats m_stats;
};

}  // namespace cr
