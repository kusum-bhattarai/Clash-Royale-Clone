// Tests for the pathfinding module.
//
// The properties worth guarding are not "it found a path" but the ones that are
// easy to get subtly wrong: that A* returns a genuinely shortest route, that
// neither search cuts a corner between two blocked tiles, and that A* and the
// flow field agree -- if they disagreed, a unit could be handed a route one of
// them considers illegal.

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "clash_royale/core/arena.hpp"
#include "clash_royale/path/astar.hpp"
#include "clash_royale/path/direct.hpp"
#include "clash_royale/path/flow_field.hpp"

using namespace cr;

namespace {

/// Blocks a fixed set of tiles, standing in for other units.
class FixedObstacles : public Obstacles {
public:
    explicit FixedObstacles(std::vector<Point> blocked) : m_blocked(std::move(blocked)) {}

    bool blocks(Point p, MovementDomain) const override {
        for (Point b : m_blocked) {
            if (b == p) {
                return true;
            }
        }
        return false;
    }

private:
    std::vector<Point> m_blocked;
};

/// Total cost of a route, charging sqrt(2) for diagonal steps.
float pathCost(Point from, const Path& path) {
    float total = 0.0f;
    Point at = from;
    for (Point step : path) {
        const bool diagonal = step.x != at.x && step.y != at.y;
        total += diagonal ? kDiagonalCost : kStraightCost;
        at = step;
    }
    return total;
}

/// True when every step is to an adjacent tile and nothing is skipped.
bool isContiguous(Point from, const Path& path) {
    Point at = from;
    for (Point step : path) {
        if (std::abs(step.x - at.x) > 1 || std::abs(step.y - at.y) > 1 || step == at) {
            return false;
        }
        at = step;
    }
    return true;
}

bool allPassable(const Arena& arena, const Path& path, MovementDomain domain) {
    for (Point step : path) {
        if (!arena.isPassable(step.x, step.y, domain)) {
            return false;
        }
    }
    return true;
}

}  // namespace

// ---------------------------------------------------------------------------
// A*
// ---------------------------------------------------------------------------

TEST(AStar, FindsTheShortestRouteAcrossOpenGround) {
    const Arena arena{20, 20};
    AStarPathfinder finder;

    const Point from{2, 2};
    const Point to{10, 6};
    const auto path = finder.findPath(arena, PathRequest{from, to, MovementDomain::Ground, nullptr});

    ASSERT_TRUE(path.has_value());
    EXPECT_EQ(path->back(), to);
    EXPECT_TRUE(isContiguous(from, *path));

    // Eight tiles across and four down: four diagonals then four straights.
    EXPECT_FLOAT_EQ(pathCost(from, *path), 4.0f * kDiagonalCost + 4.0f * kStraightCost);
}

TEST(AStar, ExcludesTheStartAndEndsOnTheGoal) {
    const Arena arena{20, 20};
    AStarPathfinder finder;

    const Point from{5, 5};
    const auto path = finder.findPath(arena, PathRequest{from, Point{5, 9}, MovementDomain::Ground, nullptr});

    ASSERT_TRUE(path.has_value());
    ASSERT_EQ(path->size(), 4u);
    EXPECT_EQ(path->front(), (Point{5, 6}));
    EXPECT_EQ(path->back(), (Point{5, 9}));
}

TEST(AStar, ReturnsAnEmptyPathWhenAlreadyThere) {
    const Arena arena{20, 20};
    AStarPathfinder finder;

    const auto path = finder.findPath(arena, PathRequest{Point{5, 5}, Point{5, 5}, MovementDomain::Ground, nullptr});
    ASSERT_TRUE(path.has_value());
    EXPECT_TRUE(path->empty());
}

TEST(AStar, RoutesGroundUnitsOverTheBridgeRatherThanThroughWater) {
    const Arena arena = Arena::standard();
    AStarPathfinder finder;
    const int river = arena.riverRows().front();

    // Straight across the middle of the river, where there is no crossing.
    const Point from{20, river - 4};
    const Point to{20, river + 4};
    const auto path = finder.findPath(arena, PathRequest{from, to, MovementDomain::Ground, nullptr});

    ASSERT_TRUE(path.has_value());
    EXPECT_TRUE(allPassable(arena, *path, MovementDomain::Ground)) << "the route crosses water";
    EXPECT_TRUE(isContiguous(from, *path));

    // It must have detoured through one of the two bridges.
    bool usedABridge = false;
    for (Point step : *path) {
        if (step.y == river) {
            EXPECT_EQ(arena.tile(step.x, step.y), Tile::Bridge);
            usedABridge = true;
        }
    }
    EXPECT_TRUE(usedABridge);
}

TEST(AStar, LetsAirUnitsFlyStraightOverTheRiver) {
    const Arena arena = Arena::standard();
    AStarPathfinder finder;
    const int river = arena.riverRows().front();

    const Point from{20, river - 4};
    const Point to{20, river + 4};
    const auto path = finder.findPath(arena, PathRequest{from, to, MovementDomain::Air, nullptr});

    ASSERT_TRUE(path.has_value());
    EXPECT_EQ(path->size(), 8u) << "an air route should be a straight line";
}

TEST(AStar, RefusesToCutTheCornerBetweenTwoBlockedTiles) {
    // A diagonal gap: (6,5) and (5,6) are walls, so (5,5) -> (6,6) would slip
    // between them. The route must go the long way round.
    Arena arena{20, 20};
    arena.setTile(6, 5, Tile::Blocked);
    arena.setTile(5, 6, Tile::Blocked);

    AStarPathfinder finder;
    const Point from{5, 5};
    const Point to{6, 6};
    const auto path = finder.findPath(arena, PathRequest{from, to, MovementDomain::Ground, nullptr});

    ASSERT_TRUE(path.has_value());
    EXPECT_TRUE(isContiguous(from, *path));
    EXPECT_GT(path->size(), 1u) << "a single diagonal step cut through the corner";
    for (Point step : *path) {
        EXPECT_NE(step, (Point{6, 5}));
        EXPECT_NE(step, (Point{5, 6}));
    }
}

TEST(AStar, ReportsFailureWhenTheGoalIsWalledOff) {
    Arena arena{20, 20};
    // Seal a 3x3 pocket around (10,10).
    for (int d = -1; d <= 1; ++d) {
        arena.setTile(9 + d, 9, Tile::Blocked);
        arena.setTile(9 + d, 11, Tile::Blocked);
        arena.setTile(9, 9 + d, Tile::Blocked);
        arena.setTile(11, 9 + d, Tile::Blocked);
    }

    AStarPathfinder finder;
    const auto path = finder.findPath(arena, PathRequest{Point{2, 2}, Point{10, 10}, MovementDomain::Ground, nullptr});
    EXPECT_FALSE(path.has_value());
}

TEST(AStar, RoutesAroundDynamicObstacles) {
    Arena arena{20, 20};
    AStarPathfinder finder;

    // A wall of "units" across row 6, with a gap at x=15.
    std::vector<Point> blocked;
    for (int x = 1; x <= 18; ++x) {
        if (x != 15) {
            blocked.push_back(Point{x, 6});
        }
    }
    const FixedObstacles obstacles{blocked};

    const Point from{3, 3};
    const Point to{3, 10};
    const auto path = finder.findPath(arena, PathRequest{from, to, MovementDomain::Ground, &obstacles});

    ASSERT_TRUE(path.has_value());
    EXPECT_TRUE(isContiguous(from, *path));
    for (Point step : *path) {
        EXPECT_FALSE(obstacles.blocks(step, MovementDomain::Ground))
            << "route passed through an occupied tile at (" << step.x << "," << step.y << ")";
    }
}

TEST(AStar, GoalTileIsReachableEvenWhenOccupied) {
    // Otherwise no melee unit could ever reach anything, since its target
    // occupies the tile it is walking to.
    Arena arena{20, 20};
    AStarPathfinder finder;

    const FixedObstacles obstacles{{Point{5, 9}}};
    const auto path =
        finder.findPath(arena, PathRequest{Point{5, 5}, Point{5, 9}, MovementDomain::Ground, &obstacles});

    ASSERT_TRUE(path.has_value());
    EXPECT_EQ(path->back(), (Point{5, 9}));
}

TEST(AStar, ReusesScratchBuffersAcrossSearches) {
    // Repeated identical searches must give identical answers; a stale buffer
    // between runs would show up here.
    const Arena arena = Arena::standard();
    AStarPathfinder finder;
    const PathRequest request{Point{5, 5}, Point{34, 28}, MovementDomain::Ground, nullptr};

    const auto first = finder.findPath(arena, request);
    ASSERT_TRUE(first.has_value());
    const std::size_t expandedFirst = finder.lastExpandedNodes();

    for (int i = 0; i < 50; ++i) {
        const auto again = finder.findPath(arena, request);
        ASSERT_TRUE(again.has_value());
        EXPECT_EQ(*again, *first) << "search " << i << " diverged";
        EXPECT_EQ(finder.lastExpandedNodes(), expandedFirst);
    }
}

TEST(AStar, ExpandsFarFewerNodesThanTheArenaHolds) {
    // Confirms the heuristic is actually guiding the search rather than it
    // degenerating into a breadth-first sweep.
    const Arena arena = Arena::standard();
    AStarPathfinder finder;

    const auto path = finder.findPath(arena, PathRequest{Point{5, 20}, Point{30, 28}, MovementDomain::Ground, nullptr});
    ASSERT_TRUE(path.has_value());

    const std::size_t cells = static_cast<std::size_t>(arena.width()) * static_cast<std::size_t>(arena.height());
    EXPECT_LT(finder.lastExpandedNodes(), cells / 2) << "expanded " << finder.lastExpandedNodes() << " of " << cells;
}

// ---------------------------------------------------------------------------
// Flow field
// ---------------------------------------------------------------------------

TEST(FlowField, CostsZeroAtTheGoalAndRisesWithDistance) {
    const Arena arena{20, 20};
    FlowField field;
    field.build(arena, Point{10, 10}, MovementDomain::Ground);

    ASSERT_TRUE(field.isBuilt());
    EXPECT_FLOAT_EQ(field.costAt(Point{10, 10}), 0.0f);
    EXPECT_FLOAT_EQ(field.costAt(Point{10, 11}), kStraightCost);
    EXPECT_FLOAT_EQ(field.costAt(Point{11, 11}), kDiagonalCost);
    EXPECT_LT(field.costAt(Point{10, 13}), field.costAt(Point{10, 16}));
}

TEST(FlowField, EveryStepDecreasesTheCost) {
    // This is what guarantees following the field terminates.
    const Arena arena = Arena::standard();
    FlowField field;
    field.build(arena, Point{19, 27}, MovementDomain::Ground);

    for (int y = 2; y < arena.height() - 2; y += 3) {
        for (int x = 2; x < arena.width() - 2; x += 3) {
            const Point at{x, y};
            if (!field.isReachable(at) || at == field.goal()) {
                continue;
            }
            const auto next = field.nextFrom(at);
            ASSERT_TRUE(next.has_value()) << "no step from (" << x << "," << y << ")";
            EXPECT_LT(field.costAt(*next), field.costAt(at));
        }
    }
}

TEST(FlowField, MarksWaterUnreachableForGroundAndReachableForAir) {
    const Arena arena = Arena::standard();
    const int river = arena.riverRows().front();

    FlowField ground;
    ground.build(arena, Point{19, 27}, MovementDomain::Ground);
    EXPECT_FALSE(ground.isReachable(Point{20, river})) << "open water should be unreachable on foot";
    EXPECT_TRUE(ground.isReachable(Point{10, river})) << "the bridge should be reachable";

    FlowField air;
    air.build(arena, Point{19, 27}, MovementDomain::Air);
    EXPECT_TRUE(air.isReachable(Point{20, river}));
}

TEST(FlowField, WalkingItProducesAContiguousRouteToTheGoal) {
    const Arena arena = Arena::standard();
    FlowField field;
    const Point goal{19, 27};
    field.build(arena, goal, MovementDomain::Ground);

    const Point from{20, 6};
    const auto path = field.pathFrom(from);

    ASSERT_TRUE(path.has_value());
    EXPECT_EQ(path->back(), goal);
    EXPECT_TRUE(isContiguous(from, *path));
    EXPECT_TRUE(allPassable(arena, *path, MovementDomain::Ground));
}

TEST(FlowField, AgreesWithAStarOnRouteCost) {
    // Both are shortest-path searches over the same cost model, so they must
    // find routes of the same length even when they pick different tiles. A
    // mismatch would mean one of them has a different idea of what is walkable.
    const Arena arena = Arena::standard();
    AStarPathfinder astar;

    const Point goal{19, 27};
    FlowField field;
    field.build(arena, goal, MovementDomain::Ground);

    const Point starts[] = {{5, 5}, {20, 6}, {34, 10}, {2, 16}, {30, 20}, {12, 30}};
    for (Point from : starts) {
        SCOPED_TRACE("from (" + std::to_string(from.x) + "," + std::to_string(from.y) + ")");

        const auto viaAStar = astar.findPath(arena, PathRequest{from, goal, MovementDomain::Ground, nullptr});
        const auto viaField = field.pathFrom(from);

        ASSERT_EQ(viaAStar.has_value(), viaField.has_value());
        if (!viaAStar.has_value()) {
            continue;
        }
        EXPECT_NEAR(pathCost(from, *viaAStar), pathCost(from, *viaField), 1e-3f);
    }
}

TEST(FlowField, ReportsUnreachableGoalsRatherThanLooping) {
    Arena arena{20, 20};
    for (int d = -1; d <= 1; ++d) {
        arena.setTile(9 + d, 9, Tile::Blocked);
        arena.setTile(9 + d, 11, Tile::Blocked);
        arena.setTile(9, 9 + d, Tile::Blocked);
        arena.setTile(11, 9 + d, Tile::Blocked);
    }

    FlowField field;
    field.build(arena, Point{10, 10}, MovementDomain::Ground);

    EXPECT_FALSE(field.isReachable(Point{2, 2}));
    EXPECT_FALSE(field.pathFrom(Point{2, 2}).has_value());
}

// ---------------------------------------------------------------------------
// Shared fields
// ---------------------------------------------------------------------------

TEST(FlowFieldPathfinder, BuildsOneFieldPerGoalNoMatterHowManyUnitsAsk) {
    // The whole reason this exists: forty units converging on three towers
    // should cost three expansions, not forty searches.
    const Arena arena = Arena::standard();
    FlowFieldPathfinder finder;

    const Point towers[] = {{19, 27}, {6, 25}, {31, 25}};
    int requests = 0;
    for (int i = 0; i < 40; ++i) {
        for (Point tower : towers) {
            const Point from{2 + (i % 30), 4 + (i % 10)};
            finder.findPath(arena, PathRequest{from, tower, MovementDomain::Ground, nullptr});
            ++requests;
        }
    }

    EXPECT_EQ(requests, 120);
    EXPECT_EQ(finder.fieldsBuilt(), 3u) << "a field was rebuilt per request instead of being shared";
}

TEST(FlowFieldPathfinder, SeparatesFieldsByDomain) {
    const Arena arena = Arena::standard();
    FlowFieldPathfinder finder;
    const Point tower{19, 27};

    finder.findPath(arena, PathRequest{Point{5, 5}, tower, MovementDomain::Ground, nullptr});
    finder.findPath(arena, PathRequest{Point{5, 5}, tower, MovementDomain::Air, nullptr});

    EXPECT_EQ(finder.fieldsBuilt(), 2u) << "ground and air must not share a field";
}

TEST(FlowFieldPathfinder, InvalidateForcesARebuild) {
    const Arena arena = Arena::standard();
    FlowFieldPathfinder finder;
    const PathRequest request{Point{5, 5}, Point{19, 27}, MovementDomain::Ground, nullptr};

    finder.findPath(arena, request);
    finder.findPath(arena, request);
    ASSERT_EQ(finder.fieldsBuilt(), 1u);

    finder.invalidate();
    EXPECT_EQ(finder.cachedFields(), 0u);

    finder.findPath(arena, request);
    EXPECT_EQ(finder.fieldsBuilt(), 2u);
}

// ---------------------------------------------------------------------------
// Direct line
// ---------------------------------------------------------------------------

TEST(DirectPathfinder, WalksAStraightLineForAirUnits) {
    const Arena arena = Arena::standard();
    DirectPathfinder finder;

    const Point from{5, 5};
    const Point to{15, 25};
    const auto path = finder.findPath(arena, PathRequest{from, to, MovementDomain::Air, nullptr});

    ASSERT_TRUE(path.has_value());
    EXPECT_EQ(path->back(), to);
    EXPECT_TRUE(isContiguous(from, *path));
    // Twenty rows to descend, so twenty steps regardless of the sideways drift.
    EXPECT_EQ(path->size(), 20u);
}

TEST(DirectPathfinder, FailsWhenTheLineIsBlockedSoCallersCanFallBack) {
    const Arena arena = Arena::standard();
    DirectPathfinder finder;

    // A ground unit cannot take the straight line across open water.
    const int river = arena.riverRows().front();
    const auto path =
        finder.findPath(arena, PathRequest{Point{20, river - 3}, Point{20, river + 3}, MovementDomain::Ground, nullptr});

    EXPECT_FALSE(path.has_value());
}

TEST(DirectPathfinder, CrossesWaterForAir) {
    const Arena arena = Arena::standard();
    DirectPathfinder finder;
    const int river = arena.riverRows().front();

    const auto path =
        finder.findPath(arena, PathRequest{Point{20, river - 3}, Point{20, river + 3}, MovementDomain::Air, nullptr});

    ASSERT_TRUE(path.has_value());
    EXPECT_EQ(path->size(), 6u);
}
