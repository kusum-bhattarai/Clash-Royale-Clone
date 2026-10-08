#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "clash_royale/path/pathfinder.hpp"

namespace cr {

/// A* over the arena grid, eight-directional.
///
/// Two details matter for correctness:
///
///   - Diagonal moves are refused when either orthogonal neighbour is blocked,
///     so units cannot slip through the corner between two obstacles.
///   - A diagonal step costs sqrt(2), and the heuristic is octile, so it stays
///     admissible on an eight-connected grid. Manhattan distance would
///     overestimate and could return a non-shortest route.
///
/// Two details matter for speed, both of which assume repeated queries on the
/// same arena:
///
///   - Scratch state is flat, indexed `y * width + x`, rather than hashed by
///     coordinate.
///   - Those buffers are allocated once and reused. Instead of clearing them
///     per search, each entry carries the generation it was written in, so a
///     stale entry is recognised rather than erased -- which makes the cost of
///     a search proportional to the nodes it expands, not to the arena size.
class AStarPathfinder final : public Pathfinder {
public:
    std::optional<Path> findPath(const Arena& arena, const PathRequest& request) override;

    /// Nodes popped from the open set during the last search. For benchmarks
    /// and for confirming the heuristic is doing its job.
    std::size_t lastExpandedNodes() const { return m_lastExpanded; }

private:
    void resize(int width, int height);

    std::vector<float> m_gScore;
    std::vector<float> m_fScore;
    std::vector<int> m_parent;
    std::vector<std::uint32_t> m_openStamp;
    std::vector<std::uint32_t> m_closedStamp;
    std::vector<int> m_heap;

    std::uint32_t m_generation = 0;
    int m_width = 0;
    int m_height = 0;
    std::size_t m_lastExpanded = 0;
};

}  // namespace cr
