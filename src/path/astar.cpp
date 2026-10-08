#include "clash_royale/path/astar.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "clash_royale/core/arena.hpp"

namespace cr {
namespace {

constexpr int kDirX[8] = {1, -1, 0, 0, 1, 1, -1, -1};
constexpr int kDirY[8] = {0, 0, 1, -1, 1, -1, 1, -1};

/// Octile distance: the exact cost of travelling between two tiles on an
/// eight-connected grid with no obstacles, which makes it an admissible and
/// well-informed heuristic.
float octileDistance(Point a, Point b) {
    const float dx = static_cast<float>(std::abs(a.x - b.x));
    const float dy = static_cast<float>(std::abs(a.y - b.y));
    return kStraightCost * (dx + dy) + (kDiagonalCost - 2.0f * kStraightCost) * std::min(dx, dy);
}

}  // namespace

void AStarPathfinder::resize(int width, int height) {
    if (width == m_width && height == m_height) {
        return;
    }
    m_width = width;
    m_height = height;

    const std::size_t cells = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    m_gScore.assign(cells, 0.0f);
    m_fScore.assign(cells, 0.0f);
    m_parent.assign(cells, -1);
    m_openStamp.assign(cells, 0);
    m_closedStamp.assign(cells, 0);

    // A fresh grid invalidates every stamp, so restart the generation counter.
    m_generation = 0;
}

std::optional<Path> AStarPathfinder::findPath(const Arena& arena, const PathRequest& request) {
    resize(arena.width(), arena.height());
    m_lastExpanded = 0;

    const Point from = request.from;
    const Point to = request.to;

    if (!arena.inInterior(from.x, from.y) || !arena.inInterior(to.x, to.y)) {
        return std::nullopt;
    }
    if (from == to) {
        return Path{};
    }
    // The goal tile is reachable even when occupied; everything else must be
    // enterable by this mover.
    if (!arena.isPassable(to.x, to.y, request.domain)) {
        return std::nullopt;
    }

    ++m_generation;
    const std::uint32_t generation = m_generation;

    const auto index = [this](Point p) {
        return static_cast<std::size_t>(p.y) * static_cast<std::size_t>(m_width) + static_cast<std::size_t>(p.x);
    };
    const auto pointOf = [this](std::size_t i) {
        return Point{static_cast<int>(i % static_cast<std::size_t>(m_width)),
                     static_cast<int>(i / static_cast<std::size_t>(m_width))};
    };
    const auto enterable = [&](Point p) {
        if (!arena.isPassable(p.x, p.y, request.domain)) {
            return false;
        }
        if (p == to) {
            return true;  // never blocked by a dynamic obstacle
        }
        return request.obstacles == nullptr || !request.obstacles->blocks(p, request.domain);
    };

    const auto cheaper = [this](int lhs, int rhs) {
        // std::push_heap builds a max-heap, so invert the comparison to pop the
        // cheapest node.
        return m_fScore[static_cast<std::size_t>(lhs)] > m_fScore[static_cast<std::size_t>(rhs)];
    };

    m_heap.clear();

    const std::size_t startIndex = index(from);
    m_gScore[startIndex] = 0.0f;
    m_fScore[startIndex] = octileDistance(from, to);
    m_parent[startIndex] = -1;
    m_openStamp[startIndex] = generation;
    m_heap.push_back(static_cast<int>(startIndex));

    const std::size_t goalIndex = index(to);
    bool found = false;

    while (!m_heap.empty()) {
        std::pop_heap(m_heap.begin(), m_heap.end(), cheaper);
        const int currentRaw = m_heap.back();
        m_heap.pop_back();

        const std::size_t current = static_cast<std::size_t>(currentRaw);
        if (m_closedStamp[current] == generation) {
            continue;  // already expanded via a cheaper route
        }
        m_closedStamp[current] = generation;
        ++m_lastExpanded;

        if (current == goalIndex) {
            found = true;
            break;
        }

        const Point at = pointOf(current);
        for (int dir = 0; dir < 8; ++dir) {
            const Point next{at.x + kDirX[dir], at.y + kDirY[dir]};
            if (!enterable(next)) {
                continue;
            }

            const bool diagonal = kDirX[dir] != 0 && kDirY[dir] != 0;
            if (diagonal) {
                // Refuse to cut the corner between two blocked tiles. Without
                // this a unit can slip diagonally between the end of a bridge
                // and the water beside it.
                if (!enterable(Point{at.x + kDirX[dir], at.y}) || !enterable(Point{at.x, at.y + kDirY[dir]})) {
                    continue;
                }
            }

            const std::size_t neighbour = index(next);
            if (m_closedStamp[neighbour] == generation) {
                continue;
            }

            const float tentative = m_gScore[current] + (diagonal ? kDiagonalCost : kStraightCost);
            const bool unseen = m_openStamp[neighbour] != generation;
            if (!unseen && tentative >= m_gScore[neighbour]) {
                continue;
            }

            m_gScore[neighbour] = tentative;
            m_fScore[neighbour] = tentative + octileDistance(next, to);
            m_parent[neighbour] = currentRaw;
            m_openStamp[neighbour] = generation;
            m_heap.push_back(static_cast<int>(neighbour));
            std::push_heap(m_heap.begin(), m_heap.end(), cheaper);
        }
    }

    if (!found) {
        return std::nullopt;
    }

    Path path;
    for (std::size_t at = goalIndex; at != startIndex;) {
        path.push_back(pointOf(at));
        const int parent = m_parent[at];
        if (parent < 0) {
            break;
        }
        at = static_cast<std::size_t>(parent);
    }
    std::reverse(path.begin(), path.end());
    return path;
}

}  // namespace cr
