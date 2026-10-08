// Compares the pathfinding strategies, so the choice between them rests on
// measurement rather than assertion.
//
// Build with -DCR_BUILD_BENCHMARKS=ON and run ./path_benchmark.
//
// What the numbers show: on the arena the game ships with, per-unit A* is
// perfectly adequate -- under a millisecond for a full frame, against a 100ms
// budget. The shared flow field matters when either the arena or the unit count
// grows, which is the case this is a library for.

#include <chrono>
#include <cstdio>
#include <vector>

#include "clash_royale/core/arena.hpp"
#include "clash_royale/path/astar.hpp"
#include "clash_royale/path/direct.hpp"
#include "clash_royale/path/flow_field.hpp"

using namespace cr;
using Clock = std::chrono::steady_clock;

namespace {

template <typename Fn>
double averageMs(int repetitions, Fn&& body) {
    const auto start = Clock::now();
    for (int i = 0; i < repetitions; ++i) {
        body();
    }
    const double total = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    return total / repetitions;
}

/// One frame of routing: `unitCount` movers each heading for one of
/// `goalCount` goals. `framesPerRebuild` of 0 means the flow fields are never
/// invalidated; 1 means they are rebuilt every frame, the worst case for
/// sharing.
void compare(const Arena& arena, int unitCount, int goalCount, int framesPerRebuild) {
    std::vector<Point> goals;
    for (int g = 0; g < goalCount; ++g) {
        goals.push_back(Point{4 + (g * 11) % (arena.width() - 8), arena.height() - 6 - (g % 3)});
    }
    std::vector<Point> units;
    for (int i = 0; i < unitCount; ++i) {
        units.push_back(Point{2 + (i * 7) % (arena.width() - 4), 2 + (i * 3) % 10});
    }

    AStarPathfinder astar;
    const double aStarMs = averageMs(100, [&] {
        for (int i = 0; i < unitCount; ++i) {
            astar.findPath(arena, PathRequest{units[i], goals[i % goalCount], MovementDomain::Ground, nullptr});
        }
    });

    FlowFieldPathfinder fields;
    int frame = 0;
    const double fieldMs = averageMs(100, [&] {
        if (framesPerRebuild > 0 && (frame++ % framesPerRebuild) == 0) {
            fields.invalidate();
        }
        for (int i = 0; i < unitCount; ++i) {
            fields.findPath(arena, PathRequest{units[i], goals[i % goalCount], MovementDomain::Ground, nullptr});
        }
    });

    std::printf("  %4d units -> %d goals, rebuild every %2d frames : A* %8.3f ms | fields %7.3f ms | %6.1fx\n",
                unitCount, goalCount, framesPerRebuild, aStarMs, fieldMs, aStarMs / fieldMs);
}

}  // namespace

int main() {
    const Arena shipped = Arena::standard();
    std::printf("Shipped arena (%dx%d), 100ms frame budget:\n", shipped.width(), shipped.height());
    compare(shipped, 40, 3, 1);
    compare(shipped, 40, 3, 10);
    compare(shipped, 40, 3, 0);

    Arena large{160, 140};
    large.carveRiver({70}, {40, 120}, 3);
    std::printf("\nLarge arena (%dx%d):\n", large.width(), large.height());
    compare(large, 200, 3, 1);
    compare(large, 200, 3, 10);
    compare(large, 200, 3, 0);

    std::printf("\nSearch effort on the shipped arena:\n");
    AStarPathfinder astar;
    astar.findPath(shipped, PathRequest{Point{5, 5}, Point{34, 30}, MovementDomain::Ground, nullptr});
    std::printf("  corner-to-corner A* : %5zu nodes expanded (of %d cells)\n", astar.lastExpandedNodes(),
                shipped.width() * shipped.height());

    FlowField field;
    field.build(shipped, Point{19, 27}, MovementDomain::Ground);
    std::printf("  one flow field      : %5zu tiles settled\n", field.expandedNodes());
    std::printf("  -- a field costs more than a single search, and pays for itself\n");
    std::printf("     once several units share it or it survives a frame.\n");

    DirectPathfinder direct;
    const PathRequest airRequest{Point{5, 5}, Point{34, 30}, MovementDomain::Air, nullptr};
    const double directMs = averageMs(20000, [&] { direct.findPath(shipped, airRequest); });
    const double astarMs = averageMs(20000, [&] { astar.findPath(shipped, airRequest); });
    std::printf("\nAir, single query: direct %.4f ms vs A* %.4f ms (%.1fx)\n", directMs, astarMs,
                astarMs / directMs);
    return 0;
}
