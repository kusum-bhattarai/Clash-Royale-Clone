#include "clash_royale/path/navigator.hpp"

namespace cr {

std::optional<Path> Navigator::route(const Arena& arena, const PathRequest& request) {
    ++m_stats.requests;

    // A request naming obstacles cannot be answered from a shared field, since
    // the obstacle set is specific to the asking unit.
    if (request.obstacles != nullptr) {
        ++m_stats.searches;
        return m_astar.findPath(arena, request);
    }

    if (request.domain == MovementDomain::Air) {
        if (auto line = m_direct.findPath(arena, request)) {
            ++m_stats.directLines;
            return line;
        }
        ++m_stats.searches;
        return m_astar.findPath(arena, request);
    }

    ++m_stats.fieldLookups;
    if (auto viaField = m_fields.findPath(arena, request)) {
        return viaField;
    }

    // The goal is unreachable across static terrain. A search will not find a
    // route either, but it reports the failure for the same cost and keeps the
    // two in agreement.
    ++m_stats.searches;
    return m_astar.findPath(arena, request);
}

void Navigator::invalidate() {
    m_fields.invalidate();
}

}  // namespace cr
