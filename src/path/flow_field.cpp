#include "clash_royale/path/flow_field.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "clash_royale/core/arena.hpp"

namespace cr {
namespace {

constexpr int kDirX[8] = {1, -1, 0, 0, 1, 1, -1, -1};
constexpr int kDirY[8] = {0, 0, 1, -1, 1, -1, 1, -1};

constexpr float kUnreachable = std::numeric_limits<float>::infinity();

}  // namespace

std::size_t FlowField::indexOf(Point p) const {
    return static_cast<std::size_t>(p.y) * static_cast<std::size_t>(m_width) + static_cast<std::size_t>(p.x);
}

bool FlowField::inBounds(Point p) const {
    return p.x >= 0 && p.x < m_width && p.y >= 0 && p.y < m_height;
}

void FlowField::build(const Arena& arena, Point goal, MovementDomain domain, const Obstacles* obstacles) {
    m_width = arena.width();
    m_height = arena.height();
    m_goal = goal;
    m_domain = domain;
    m_expanded = 0;
    m_built = false;

    const std::size_t cells = static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);
    m_cost.assign(cells, kUnreachable);
    m_next.assign(cells, -1);

    if (!arena.inInterior(goal.x, goal.y)) {
        return;
    }
    m_built = true;

    const auto enterable = [&](Point p) {
        if (!arena.isPassable(p.x, p.y, domain)) {
            return false;
        }
        if (p == goal) {
            return true;  // the goal tile is reachable even when occupied
        }
        return obstacles == nullptr || !obstacles->blocks(p, domain);
    };

    // Dijkstra outward from the goal. Costs are symmetric, so the distance
    // found from the goal is also the distance back to it, and the predecessor
    // recorded during expansion is exactly the step a unit should take toward
    // the goal.
    std::vector<int> heap;
    const auto cheaper = [this](int lhs, int rhs) {
        return m_cost[static_cast<std::size_t>(lhs)] > m_cost[static_cast<std::size_t>(rhs)];
    };

    const std::size_t goalIndex = indexOf(goal);
    m_cost[goalIndex] = 0.0f;
    heap.push_back(static_cast<int>(goalIndex));

    std::vector<bool> settled(cells, false);

    while (!heap.empty()) {
        std::pop_heap(heap.begin(), heap.end(), cheaper);
        const int currentRaw = heap.back();
        heap.pop_back();

        const std::size_t current = static_cast<std::size_t>(currentRaw);
        if (settled[current]) {
            continue;
        }
        settled[current] = true;
        ++m_expanded;

        const Point at{static_cast<int>(current % static_cast<std::size_t>(m_width)),
                       static_cast<int>(current / static_cast<std::size_t>(m_width))};

        for (int dir = 0; dir < 8; ++dir) {
            const Point next{at.x + kDirX[dir], at.y + kDirY[dir]};
            if (!inBounds(next) || !enterable(next)) {
                continue;
            }

            const bool diagonal = kDirX[dir] != 0 && kDirY[dir] != 0;
            if (diagonal) {
                // Same corner rule as A*, so the two agree about what is
                // walkable and a unit never gets a route one of them rejects.
                if (!enterable(Point{at.x + kDirX[dir], at.y}) || !enterable(Point{at.x, at.y + kDirY[dir]})) {
                    continue;
                }
            }

            const std::size_t neighbour = indexOf(next);
            if (settled[neighbour]) {
                continue;
            }

            const float candidate = m_cost[current] + (diagonal ? kDiagonalCost : kStraightCost);
            if (candidate < m_cost[neighbour]) {
                m_cost[neighbour] = candidate;
                m_next[neighbour] = currentRaw;  // step from `next` toward the goal
                heap.push_back(static_cast<int>(neighbour));
                std::push_heap(heap.begin(), heap.end(), cheaper);
            }
        }
    }
}

float FlowField::costAt(Point p) const {
    if (!m_built || !inBounds(p)) {
        return kUnreachable;
    }
    return m_cost[indexOf(p)];
}

bool FlowField::isReachable(Point p) const {
    return std::isfinite(costAt(p));
}

std::optional<Point> FlowField::nextFrom(Point p) const {
    if (!m_built || !inBounds(p) || p == m_goal) {
        return std::nullopt;
    }
    const int next = m_next[indexOf(p)];
    if (next < 0) {
        return std::nullopt;
    }
    return Point{static_cast<int>(static_cast<std::size_t>(next) % static_cast<std::size_t>(m_width)),
                 static_cast<int>(static_cast<std::size_t>(next) / static_cast<std::size_t>(m_width))};
}

std::optional<Path> FlowField::pathFrom(Point p) const {
    if (!m_built || !isReachable(p)) {
        return std::nullopt;
    }

    Path path;
    Point at = p;
    // The cost strictly decreases every step, so this cannot loop; the bound is
    // belt and braces against a malformed field.
    const std::size_t limit = m_cost.size() + 1;
    while (at != m_goal && path.size() < limit) {
        const std::optional<Point> next = nextFrom(at);
        if (!next.has_value()) {
            return std::nullopt;
        }
        path.push_back(*next);
        at = *next;
    }
    return path;
}

std::optional<Path> FlowFieldPathfinder::findPath(const Arena& arena, const PathRequest& request) {
    const Key key{request.to.x, request.to.y, request.domain};

    auto it = m_fields.find(key);
    if (it == m_fields.end()) {
        FlowField field;
        field.build(arena, request.to, request.domain, request.obstacles);
        ++m_fieldsBuilt;
        it = m_fields.emplace(key, std::move(field)).first;
    }

    return it->second.pathFrom(request.from);
}

void FlowFieldPathfinder::invalidate() {
    m_fields.clear();
}

}  // namespace cr
