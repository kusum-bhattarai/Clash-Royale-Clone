#pragma once

#include <cstddef>
#include <optional>
#include <unordered_map>
#include <vector>

#include "clash_royale/path/pathfinder.hpp"

namespace cr {

/// Distance-to-goal for every tile, with the step that gets you there.
///
/// Built by one Dijkstra expansion outward from the goal. The point is sharing:
/// in this game dozens of units converge on the same three towers, and running
/// a separate A* per unit repeats almost identical work. One field answers
/// "which way from here" for every tile at once, so N units cost one expansion
/// rather than N searches.
class FlowField {
public:
    /// Expands from `goal` across everything a mover of `domain` can enter.
    void build(const Arena& arena, Point goal, MovementDomain domain, const Obstacles* obstacles = nullptr);

    bool isBuilt() const { return m_built; }
    Point goal() const { return m_goal; }
    MovementDomain domain() const { return m_domain; }

    /// Cost from `p` to the goal, or infinity when unreachable.
    float costAt(Point p) const;

    bool isReachable(Point p) const;

    /// The tile to step to from `p`, or nullopt at the goal or when
    /// unreachable.
    std::optional<Point> nextFrom(Point p) const;

    /// The whole route from `p`, by walking the field one step at a time.
    std::optional<Path> pathFrom(Point p) const;

    /// Tiles settled during the last build, for benchmarks.
    std::size_t expandedNodes() const { return m_expanded; }

private:
    std::size_t indexOf(Point p) const;
    bool inBounds(Point p) const;

    std::vector<float> m_cost;
    std::vector<int> m_next;  ///< index of the tile to step to, or -1
    int m_width = 0;
    int m_height = 0;
    Point m_goal{};
    MovementDomain m_domain = MovementDomain::Ground;
    bool m_built = false;
    std::size_t m_expanded = 0;
};

/// A Pathfinder that answers from shared flow fields, one per goal.
///
/// Fields are cached across calls and across units, so the second and later
/// requests for a goal cost a walk of an existing field rather than a search.
/// Call invalidate() when the terrain or the obstacle set changes.
class FlowFieldPathfinder final : public Pathfinder {
public:
    std::optional<Path> findPath(const Arena& arena, const PathRequest& request) override;

    /// Discards every cached field. Cheap, and required whenever obstacles move.
    void invalidate();

    /// How many Dijkstra expansions have been run since construction. Compare
    /// against the number of requests to see the sharing at work.
    std::size_t fieldsBuilt() const { return m_fieldsBuilt; }
    std::size_t cachedFields() const { return m_fields.size(); }

private:
    struct Key {
        int x;
        int y;
        MovementDomain domain;
        bool operator==(const Key& other) const {
            return x == other.x && y == other.y && domain == other.domain;
        }
    };
    struct KeyHash {
        std::size_t operator()(const Key& key) const {
            const std::size_t packed = (static_cast<std::size_t>(static_cast<unsigned>(key.x)) << 20) ^
                                       (static_cast<std::size_t>(static_cast<unsigned>(key.y)) << 4) ^
                                       static_cast<std::size_t>(key.domain);
            return packed * 1125899906842597ULL;
        }
    };

    std::unordered_map<Key, FlowField, KeyHash> m_fields;
    std::size_t m_fieldsBuilt = 0;
};

}  // namespace cr
