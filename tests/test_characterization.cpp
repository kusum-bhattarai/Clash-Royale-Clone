// Characterization tests.
//
// These pin down the game's CURRENT observable behavior so the upcoming
// refactor (namespacing, data-driven cards, arena terrain, A*/flow-field
// pathfinding) can be verified as behavior-preserving where it is meant to be,
// and as a deliberate change where it is not.
//
// Some of these document behavior that is arguably WRONG. Those are marked
// `Bug_` and carry a comment explaining the defect. They are expected to be
// rewritten -- not silently deleted -- when the corresponding fix lands, so the
// change shows up in a diff rather than disappearing.

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string_view>

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <string>

#include "clash_royale/core/rng.hpp"
#include "clash_royale/core/types.hpp"
#include "test_helpers.hpp"
#include "clash_royale/sim/board.hpp"
#include "clash_royale/sim/simulation.hpp"
#include "clash_royale/tui/renderer.hpp"
#include "clash_royale/sim/entity.hpp"

using namespace cr;
using namespace cr::testing;

namespace {

// Drives an entity for `ticks` updates of the fixed 0.1s game step.
void tick(const std::shared_ptr<Entity>& entity, const Board& board, int ticks) {
    for (int i = 0; i < ticks; ++i) {
        entity->update(board);
    }
}

// Builds a board holding `self` plus a single enemy at (ex, ey), so movement
// can be observed against exactly one target with no ambiguity.
//
// The arena has no terrain, which matters: on the shipped arena a ground unit
// whose target is across the river steers for a bridge instead of the target,
// so a board with a river measures routing rather than movement shape. River
// behavior has its own section below.
Board boardWith(const std::shared_ptr<Entity>& self, std::string_view enemyCard, int ex, int ey) {
    Board board = openBoard();
    board.addEntity(self);
    board.addEntity(spawn(enemyCard, ex, ey, !self->getIsPlayer()));
    return board;
}

// As boardWith, but on the shipped arena, so terrain is in play.
Board terrainBoardWith(const std::shared_ptr<Entity>& self, std::string_view enemyCard, int ex, int ey) {
    Board board;  // Arena::standard()
    board.addEntity(self);
    board.addEntity(spawn(enemyCard, ex, ey, !self->getIsPlayer()));
    return board;
}

}  // namespace

// ---------------------------------------------------------------------------
// Arena geometry
//
// Board dimensions currently live on the Renderer, and the simulation reaches
// into them to clamp coordinates. Phase 4 moves this ownership to an Arena;
// these values must survive that move.
// ---------------------------------------------------------------------------

TEST(ArenaGeometry, BoardDimensionsAreOddSoTheInteriorHasATrueCentre) {
    EXPECT_EQ(kArenaWidth, 41);
    EXPECT_EQ(kArenaHeight, 35);

    // The playable interior runs from 1 to extent - 2. Both spans must have an
    // odd length, so there is a single middle column and row for the towers and
    // the river to sit on. With an even width the interior was centred on 19.5
    // while everything was placed about 20, which showed up on screen as the
    // whole board sitting a column right of its own frame.
    const int interiorWidth = kArenaWidth - 2;
    const int interiorHeight = kArenaHeight - 2;
    EXPECT_EQ(interiorWidth % 2, 1) << "no centre column";
    EXPECT_EQ(interiorHeight % 2, 1) << "no centre row";

    // And that centre is where the arena puts its midline.
    EXPECT_EQ(1 + interiorWidth / 2, kArenaWidth / 2);
    EXPECT_EQ(1 + interiorHeight / 2, kArenaHeight / 2);
}

TEST(ArenaGeometry, TowerLayoutIsMirroredOnBothAxes) {
    Simulation sim;
    const int river = sim.arena().riverRows().front();
    const int centre = kArenaWidth / 2;

    // For every tower one player owns, the other must own its mirror image.
    // Without this, a match between equal opponents is decided by side.
    for (const auto& entity : sim.board().getEntities()) {
        const int mirroredX = 2 * centre - entity->getX();
        const int mirroredY = 2 * river - entity->getY();

        bool foundMirror = false;
        for (const auto& other : sim.board().getEntities()) {
            if (other->getIsPlayer() == entity->getIsPlayer()) {
                continue;
            }
            if (other->getX() == mirroredX && other->getY() == mirroredY &&
                other->cardId() == entity->cardId()) {
                foundMirror = true;
                break;
            }
        }
        EXPECT_TRUE(foundMirror) << entity->cardId() << " at (" << entity->getX() << "," << entity->getY()
                                 << ") has no mirror on the other side";
    }
}

TEST(ArenaGeometry, BothPlayersSpawnTheSameDistanceFromEnemyTowers) {
    Simulation sim;

    double distances[2] = {0.0, 0.0};
    for (bool isPlayerOne : {true, false}) {
        const SpawnPoint spawnAt = spawnPointFor(defaultCards().get(cards::Knight), Lane::LEFT, isPlayerOne);
        int nearestSq = std::numeric_limits<int>::max();
        for (const auto& entity : sim.board().getEntities()) {
            if (entity->getIsPlayer() == isPlayerOne || entity->spec().towerRole == TowerRole::None) {
                continue;
            }
            const int dx = entity->getX() - spawnAt.x;
            const int dy = entity->getY() - spawnAt.y;
            nearestSq = std::min(nearestSq, dx * dx + dy * dy);
        }
        distances[isPlayerOne ? 0 : 1] = std::sqrt(static_cast<double>(nearestSq));
    }

    // Player two used to spawn 11.70 tiles from the nearest enemy tower where
    // player one spawned 15.52 away.
    EXPECT_NEAR(distances[0], distances[1], 0.01) << "one side has a shorter run at the other's towers";
}

TEST(ArenaGeometry, MatchStartsWithSixTowersAtFixedPositions) {
    Simulation sim;
    const auto& entities = sim.board().getEntities();

    ASSERT_EQ(entities.size(), 6u);

    struct Expected {
        std::string_view card;
        int x, y;
        bool isPlayer;
    };
    // Mirrored about x = 20 and about the river row 17. The original layout was
    // asymmetric both ways -- player two's towers sat 14 and 12 rows from the
    // midline against player one's 10 and 8, and the left queens were a tile
    // further out than the right -- which measurably favoured player one.
    const Expected expected[] = {
        {cards::KingTower, 20, 7, false},
        {cards::QueenTower, 8, 9, false},
        {cards::QueenTower, 32, 9, false},
        {cards::KingTower, 20, 27, true},
        {cards::QueenTower, 8, 25, true},
        {cards::QueenTower, 32, 25, true},
    };

    for (size_t i = 0; i < std::size(expected); ++i) {
        SCOPED_TRACE("tower index " + std::to_string(i));
        EXPECT_EQ(entities[i]->cardId(), expected[i].card);
        EXPECT_EQ(entities[i]->getX(), expected[i].x);
        EXPECT_EQ(entities[i]->getY(), expected[i].y);
        EXPECT_EQ(entities[i]->getIsPlayer(), expected[i].isPlayer);
    }
}

TEST(ArenaGeometry, MatchStartsWithFiveElixirEach) {
    Simulation sim;
    EXPECT_FLOAT_EQ(sim.elixir(true), 5.0f);
    EXPECT_FLOAT_EQ(sim.elixir(false), 5.0f);
    EXPECT_TRUE(sim.isRunning());
}

// ---------------------------------------------------------------------------
// Entity construction invariants
// ---------------------------------------------------------------------------

TEST(EntityConstruction, ClampsPositionsOutsideTheArena) {
    const CardSpec& knight = defaultCards().get(cards::Knight);

    Entity tooLow(knight, standardArena(), -5, -5, true, Lane::LEFT);
    EXPECT_EQ(tooLow.getX(), 1);
    EXPECT_EQ(tooLow.getY(), 1);

    Entity tooHigh(knight, standardArena(), 999, 999, true, Lane::LEFT);
    EXPECT_EQ(tooHigh.getX(), kArenaWidth - 2);   // 38
    EXPECT_EQ(tooHigh.getY(), kArenaHeight - 2);  // 33
}

TEST(EntityConstruction, RaisesNonPositiveHealthToOne) {
    // Health comes from the card now, so an invalid value has to be defined as
    // one. This doubles as the smallest possible custom-card registration.
    CardRegistry registry;
    CardSpec broken;
    broken.id = "broken";
    broken.symbol = 'X';
    broken.health = 0;

    Entity entity(registry.define(broken), standardArena(), 10, 10, true, Lane::LEFT);

    EXPECT_EQ(entity.getHealth(), 1);
    // The maximum is corrected too. It previously kept the invalid value while
    // only the current health was clamped, which left a zero denominator for
    // the renderer's health-bar arithmetic.
    EXPECT_EQ(entity.getMaxHealth(), 1);
    EXPECT_TRUE(entity.isAlive());
}

// ---------------------------------------------------------------------------
// Movement cadence
//
// Entity::update() accumulates 0.1 per call and steps once the accumulator
// reaches 1.0 / moveSpeed. These tick counts are the exact thresholds, and they
// bake the 0.1s timestep into the entity itself -- Phase 2 replaces this with an
// explicit dt, which must reproduce the same cadence at the same step size.
// ---------------------------------------------------------------------------

struct CadenceCase {
    std::string_view card;
    int ticksPerStep;
};

class MovementCadence : public ::testing::TestWithParam<CadenceCase> {};

TEST_P(MovementCadence, StepsOnExpectedTick) {
    const CadenceCase param = GetParam();

    auto unit = spawn(param.card, 10, 10, true);
    // Must be a tower: the Golem ignores troops entirely, so a troop decoy
    // would leave it with no target and no movement at all.
    Board board = boardWith(unit, cards::QueenTower, 10, 30);
    const int startY = unit->getY();

    tick(unit, board, param.ticksPerStep - 1);
    EXPECT_EQ(unit->getY(), startY) << "moved one tick too early";

    tick(unit, board, 1);
    EXPECT_NE(unit->getY(), startY) << "failed to move on the expected tick";
}

INSTANTIATE_TEST_SUITE_P(
    AllUnits, MovementCadence,
    ::testing::Values(CadenceCase{cards::Golem, 20},    // speed 0.50
                      CadenceCase{cards::Pekka, 14},    // speed 0.75
                      CadenceCase{cards::Knight, 10},   // speed 1.00
                      CadenceCase{cards::Wizard, 10},   // speed 1.00
                      CadenceCase{cards::Goblins, 9},   // speed 1.20
                      CadenceCase{cards::Archers, 9},   // speed 1.20
                      CadenceCase{cards::Dragon, 7}));  // speed 1.50

TEST(MovementCadence, StationaryUnitsNeverStep) {
    for (std::string_view card : {cards::KingTower, cards::QueenTower, cards::Canon}) {
        auto building = spawn(card, 10, 10, true);
        Board board = boardWith(building, cards::Knight, 10, 30);

        tick(building, board, 100);

        EXPECT_EQ(building->getX(), 10);
        EXPECT_EQ(building->getY(), 10);
    }
}

// ---------------------------------------------------------------------------
// Movement shape
//
// Each unit's per-step displacement. Phase 5 replaces straight-line chasing
// with pathfinding, but the per-unit "flavor" captured here is meant to be
// preserved as a steering layer on top of the path.
// ---------------------------------------------------------------------------

TEST(MovementShape, GroundUnitsMoveOnOneAxisAndPreferVerticalOnTies) {
    auto knight = spawn(cards::Knight, 10, 10, true);
    // dx == dy == 10, so |dx| > |dy| is false and the tie resolves vertically.
    Board board = boardWith(knight, cards::Knight, 20, 20);

    tick(knight, board, 10);

    EXPECT_EQ(knight->getX(), 10);
    EXPECT_EQ(knight->getY(), 11);
}

TEST(MovementShape, GroundUnitsMoveHorizontallyWhenHorizontalDistanceDominates) {
    auto knight = spawn(cards::Knight, 10, 10, true);
    Board board = boardWith(knight, cards::Knight, 30, 12);

    tick(knight, board, 10);

    EXPECT_EQ(knight->getX(), 11);
    EXPECT_EQ(knight->getY(), 10);
}

TEST(MovementShape, DragonMovesDiagonallyOnBothAxesAtOnce) {
    auto dragon = spawn(cards::Dragon, 10, 10, true);
    Board board = boardWith(dragon, cards::Knight, 20, 20);

    tick(dragon, board, 7);

    EXPECT_EQ(dragon->getX(), 11);
    EXPECT_EQ(dragon->getY(), 11);
}

TEST(MovementShape, RangedUnitsHaltOnceTheTargetIsWithinAttackRange) {
    // Archers have range 7. Starting 10 tiles away, they close to exactly 7 and
    // then stop advancing.
    auto archers = spawn(cards::Archers, 10, 10, true);
    Board board = boardWith(archers, cards::Knight, 10, 20);

    tick(archers, board, 200);

    EXPECT_EQ(archers->getX(), 10);
    EXPECT_EQ(archers->getY(), 13) << "archers should stop at exactly their attack range";
}

TEST(MovementShape, UnitsWithNoEnemyOnTheBoardDoNotMove) {
    Board board;
    auto knight = spawn(cards::Knight, 10, 10, true);
    board.addEntity(knight);
    board.addEntity(spawn(cards::Knight, 10, 20, true));

    tick(knight, board, 100);

    EXPECT_EQ(knight->getX(), 10);
    EXPECT_EQ(knight->getY(), 10);
}

// Goblins alternate which axis they close on. This branch existed from the
// start but was unreachable: it keyed on `m_moveTimer`, which Entity::update()
// zeroes immediately before calling move(), so only the horizontal-first case
// ever ran. It now keys on the movement step count.
TEST(MovementShape, GoblinsAlternateAxesAsTheyClose) {
    auto goblins = spawn(cards::Goblins, 10, 10, true);
    Board board = boardWith(goblins, cards::Knight, 20, 20);

    const int kTicksPerStep = 9;

    // Step 0 is horizontal, step 1 vertical, step 2 horizontal again.
    tick(goblins, board, kTicksPerStep);
    EXPECT_EQ(goblins->getX(), 11);
    EXPECT_EQ(goblins->getY(), 10);

    tick(goblins, board, kTicksPerStep);
    EXPECT_EQ(goblins->getX(), 11) << "second step should have gone vertical";
    EXPECT_EQ(goblins->getY(), 11);

    tick(goblins, board, kTicksPerStep);
    EXPECT_EQ(goblins->getX(), 12);
    EXPECT_EQ(goblins->getY(), 11);
}

// ---------------------------------------------------------------------------
// River and bridges
//
// The arena gained terrain, which is what finally gives `Lane` an effect: it
// was recorded on every entity and read by nothing, because the field was open.
// ---------------------------------------------------------------------------

TEST(ArenaTerrain, StandardArenaHasARiverOnTheMidlineWithABridgePerLane) {
    const Arena& arena = standardArena();

    ASSERT_TRUE(arena.hasRiver());
    ASSERT_EQ(arena.riverRows().size(), 1u);
    EXPECT_EQ(arena.riverRows().front(), kArenaHeight / 2);  // 17

    // Bridges sit on the lane spawn columns.
    ASSERT_EQ(arena.bridgeColumns().size(), 2u);
    EXPECT_EQ(arena.bridgeColumns().front(), kArenaWidth / 4);      // 10
    EXPECT_EQ(arena.bridgeColumns().back(), kArenaWidth * 3 / 4);   // 30
}

TEST(ArenaTerrain, RiverIsWaterExceptWhereTheBridgesCross) {
    const Arena& arena = standardArena();
    const int row = arena.riverRows().front();

    // Three tiles wide, centred on each lane column.
    for (int x : {9, 10, 11, 29, 30, 31}) {
        EXPECT_EQ(arena.tile(x, row), Tile::Bridge) << "x = " << x;
    }
    for (int x : {1, 8, 12, 20, 28, 32, 38}) {
        EXPECT_EQ(arena.tile(x, row), Tile::Water) << "x = " << x;
    }
}

TEST(ArenaTerrain, GroundUnitsCannotEnterWaterButAirCan) {
    const Arena& arena = standardArena();
    const int row = arena.riverRows().front();

    EXPECT_FALSE(arena.isPassable(20, row, MovementDomain::Ground));
    EXPECT_TRUE(arena.isPassable(20, row, MovementDomain::Air));

    // A bridge is walkable by both.
    EXPECT_TRUE(arena.isPassable(10, row, MovementDomain::Ground));
    EXPECT_TRUE(arena.isPassable(10, row, MovementDomain::Air));

    // The border is impassable to everything.
    EXPECT_FALSE(arena.isPassable(0, 5, MovementDomain::Air));
    EXPECT_FALSE(arena.isPassable(5, 0, MovementDomain::Air));
}

TEST(ArenaTerrain, NoFixedPositionLandsInTheRiver) {
    // The river is one row wide because that is the only placement clear of
    // every tower, troop spawn and Canon spawn. Widening it would drop a
    // building into the water.
    const Arena& arena = standardArena();
    Simulation sim;

    for (const auto& entity : sim.board().getEntities()) {
        EXPECT_NE(arena.tile(entity->getX(), entity->getY()), Tile::Water)
            << entity->cardId() << " starts in the river";
    }

    for (Lane lane : {Lane::LEFT, Lane::RIGHT}) {
        for (bool isPlayerOne : {true, false}) {
            for (std::string_view card : {cards::Knight, cards::Canon}) {
                const SpawnPoint point = spawnPointFor(defaultCards().get(card), lane, isPlayerOne);
                EXPECT_NE(arena.tile(point.x, point.y), Tile::Water)
                    << card << " spawns in the river";
            }
        }
    }
}

TEST(RiverRouting, GroundUnitsSteerForTheirLaneBridgeRatherThanStraightAcross) {
    // A Knight in the left lane, with its target directly across the river in
    // the right lane. Without terrain it would walk straight at the target and
    // into the water.
    auto knight = spawn(cards::Knight, 10, 12, true, Lane::LEFT);
    Board board = terrainBoardWith(knight, cards::QueenTower, 31, 25);

    tick(knight, board, 400);

    // It must have crossed, and done so over its own bridge.
    EXPECT_GT(knight->getY(), standardArena().riverRows().front())
        << "the knight never got across the river";
}

TEST(RiverRouting, LaneDecidesWhichCrossingIsUsedViaTheSpawnPoint) {
    // Lane no longer steers routing directly. It picks the spawn column, and
    // the pathfinder then crosses at whichever bridge is nearest -- which, for
    // a unit deployed in a lane, is that lane's own bridge.
    //
    // That is both simpler and closer to the real game: a lane is where you
    // deploy, not an instruction handed to the unit. It also means a unit
    // pushed off its lane re-routes sensibly instead of marching back.
    const int riverRow = standardArena().riverRows().front();

    for (const auto& [lane, expectedColumn] :
         {std::pair{Lane::LEFT, kArenaWidth / 4}, std::pair{Lane::RIGHT, kArenaWidth * 3 / 4}}) {
        const SpawnPoint spawnAt = spawnPointFor(defaultCards().get(cards::Knight), lane, /*isPlayerOne=*/false);
        auto knight = spawn(cards::Knight, spawnAt.x, spawnAt.y, false, lane);
        Board board = terrainBoardWith(knight, cards::KingTower, 19, 27);

        int crossingColumn = -1;
        for (int i = 0; i < 400 && crossingColumn < 0; ++i) {
            knight->update(board);
            if (knight->getY() == riverRow) {
                crossingColumn = knight->getX();
            }
        }

        ASSERT_GE(crossingColumn, 0) << "the unit never reached the river";
        EXPECT_NEAR(crossingColumn, expectedColumn, 1) << "crossed at the wrong bridge";
    }
}

TEST(RiverRouting, UnitsCrossAtWhicheverBridgeIsNearest) {
    const int riverRow = standardArena().riverRows().front();

    // Far left and far right, both nominally in the LEFT lane. Each should take
    // the crossing closest to it rather than the one its lane names.
    for (const auto& [startX, expectedColumn] :
         {std::pair{3, kArenaWidth / 4}, std::pair{36, kArenaWidth * 3 / 4}}) {
        auto knight = spawn(cards::Knight, startX, 10, false, Lane::LEFT);
        Board board = terrainBoardWith(knight, cards::KingTower, 19, 27);

        int crossingColumn = -1;
        for (int i = 0; i < 600 && crossingColumn < 0; ++i) {
            knight->update(board);
            if (knight->getY() == riverRow) {
                crossingColumn = knight->getX();
            }
        }

        ASSERT_GE(crossingColumn, 0) << "the unit never reached the river from x=" << startX;
        EXPECT_NEAR(crossingColumn, expectedColumn, 1)
            << "a unit starting at x=" << startX << " did not take the nearest bridge";
    }
}

TEST(RiverRouting, GroundUnitsNeverStandOnWater) {
    auto knight = spawn(cards::Knight, 4, 12, true, Lane::LEFT);
    Board board = terrainBoardWith(knight, cards::QueenTower, 31, 25);

    for (int i = 0; i < 600; ++i) {
        knight->update(board);
        ASSERT_NE(standardArena().tile(knight->getX(), knight->getY()), Tile::Water)
            << "knight entered the water at (" << knight->getX() << "," << knight->getY() << ")";
    }
}

TEST(RiverRouting, AirUnitsIgnoreTheRiverEntirely) {
    // A Dragon takes the straight diagonal, crossing wherever it likes.
    auto dragon = spawn(cards::Dragon, 20, 12, true, Lane::LEFT);
    Board board = terrainBoardWith(dragon, cards::QueenTower, 31, 25);

    bool crossedOverWater = false;
    for (int i = 0; i < 200; ++i) {
        dragon->update(board);
        if (standardArena().tile(dragon->getX(), dragon->getY()) == Tile::Water) {
            crossedOverWater = true;
        }
    }

    EXPECT_TRUE(crossedOverWater) << "the dragon detoured to a bridge instead of flying over";
    EXPECT_GT(dragon->getY(), standardArena().riverRows().front());
}

TEST(RiverRouting, AnArenaWithoutARiverNeedsNoRouting) {
    // Terrain is optional: an open arena behaves exactly as before.
    auto knight = spawn(cards::Knight, 10, 10, true, Lane::RIGHT);
    Board board = boardWith(knight, cards::QueenTower, 10, 30);

    tick(knight, board, 10);

    // Straight at the target, with no detour toward the right-lane bridge.
    EXPECT_EQ(knight->getX(), 10);
    EXPECT_EQ(knight->getY(), 11);
}

// ---------------------------------------------------------------------------
// Routing, collision and route caching
//
// Movement now follows a route from a Pathfinder rather than stepping greedily.
// What matters here is the behavior that emerges from that: units avoid each
// other, stop when they are close enough to fight, and do not re-search on
// every step.
// ---------------------------------------------------------------------------

TEST(Routing, EveryUnitStopsOnceItsTargetIsInReach) {
    // Previously only ranged units held back; a melee unit walked onto the tile
    // its target occupied.
    auto knight = spawn(cards::Knight, 10, 10, true);
    Board board = boardWith(knight, cards::QueenTower, 10, 20);

    tick(knight, board, 400);

    const auto& tower = board.getEntities().back();
    EXPECT_NE(knight->getY(), tower->getY()) << "the knight ended up standing on its target";
    EXPECT_TRUE(isWithinRange(*knight, *tower, knight->getAttackRange()))
        << "the knight stopped before it could attack";
}

TEST(Routing, RangedUnitsStopFurtherOutThanMeleeOnes) {
    auto archers = spawn(cards::Archers, 10, 2, true);
    Board archerBoard = boardWith(archers, cards::QueenTower, 10, 25);
    tick(archers, archerBoard, 600);

    auto knight = spawn(cards::Knight, 10, 2, true);
    Board knightBoard = boardWith(knight, cards::QueenTower, 10, 25);
    tick(knight, knightBoard, 600);

    // Archers have range 7, the Knight 1, so the Knight closes much further.
    EXPECT_LT(archers->getY(), knight->getY()) << "the archers closed as far as the melee unit";
}

TEST(Collision, GroundUnitsDoNotWalkThroughEachOther) {
    Board board = openBoard();
    auto walker = spawn(cards::Knight, 10, 10, true);
    board.addEntity(walker);
    // A wall of friendly units directly between the walker and its target.
    for (int x = 8; x <= 12; ++x) {
        board.addEntity(spawn(cards::Knight, x, 11, true));
    }
    board.addEntity(spawn(cards::QueenTower, 10, 20, false));

    for (int i = 0; i < 200; ++i) {
        board.updateEntities(0.1f);
        for (const auto& other : board.getEntities()) {
            if (other == walker || other->isFlying() || other->spec().isBuilding) {
                continue;
            }
            EXPECT_FALSE(walker->getX() == other->getX() && walker->getY() == other->getY())
                << "two ground units occupied the same tile";
        }
    }
}

TEST(Collision, GroundAndAirUnitsDoNotBlockEachOther) {
    // A flier and a ground troop share a plane only visually.
    Board board = openBoard();
    auto dragon = spawn(cards::Dragon, 10, 10, true);
    board.addEntity(dragon);
    // Wall off the row below with ground units.
    for (int x = 8; x <= 12; ++x) {
        board.addEntity(spawn(cards::Knight, x, 11, true));
    }
    board.addEntity(spawn(cards::QueenTower, 10, 20, false));

    tick(dragon, board, 100);

    EXPECT_GT(dragon->getY(), 11) << "the dragon was blocked by ground units";
}

TEST(Collision, UnitsRouteAroundACongestedChokepoint) {
    // A full-width wall of units with a single gap. The walker must find it
    // rather than grinding against the wall, which is what the congestion
    // fallback exists for.
    Board board = openBoard();
    auto walker = spawn(cards::Knight, 4, 4, true);
    board.addEntity(walker);
    for (int x = 1; x <= 38; ++x) {
        if (x != 30) {
            board.addEntity(spawn(cards::Knight, x, 10, true));
        }
    }
    board.addEntity(spawn(cards::QueenTower, 4, 20, false));

    for (int i = 0; i < 1200; ++i) {
        board.updateEntities(0.1f);
    }

    EXPECT_GT(walker->getY(), 10) << "the walker never found the gap in the wall";
}

TEST(Collision, BuildingsObstructGroundMovement) {
    Board board = openBoard();
    auto knight = spawn(cards::Knight, 10, 10, true);
    board.addEntity(knight);
    board.addEntity(spawn(cards::Canon, 10, 11, true));  // friendly, directly in the way
    board.addEntity(spawn(cards::QueenTower, 10, 20, false));

    for (int i = 0; i < 300; ++i) {
        board.updateEntities(0.1f);
        EXPECT_FALSE(knight->getX() == 10 && knight->getY() == 11) << "the knight walked through a building";
    }
    EXPECT_GT(knight->getY(), 11) << "the knight never got past the building";
}

TEST(Routing, RoutesAreCachedRatherThanRecomputedEveryStep) {
    // Re-searching every step is most of the cost of pathfinding. A route
    // survives until it runs out, the target drifts, or the repath timer fires.
    Board board;  // Arena::standard(), so the route is a real one via a bridge
    auto knight = spawn(cards::Knight, 10, 6, false, Lane::LEFT);
    board.addEntity(knight);
    board.addEntity(spawn(cards::KingTower, 19, 27, true));

    const int kSteps = 400;
    for (int i = 0; i < kSteps; ++i) {
        knight->update(board, 0.1f);
    }

    // A Knight steps once per 10 ticks, so 400 ticks is about 40 steps. With a
    // static target, routing should be asked far less often than that.
    const std::size_t requests = board.navigator().stats().requests;
    EXPECT_GT(requests, 0u);
    EXPECT_LT(requests, 20u) << "routed " << requests << " times in " << kSteps << " ticks";
}

TEST(Routing, GroundRequestsAreAnsweredFromSharedFields) {
    Board board;
    board.addEntity(spawn(cards::KingTower, 19, 27, true));
    // Several units converging on the same tower should share one field.
    for (int i = 0; i < 8; ++i) {
        board.addEntity(spawn(cards::Knight, 4 + i * 3, 6, false, Lane::LEFT));
    }

    for (int i = 0; i < 60; ++i) {
        board.updateEntities(0.1f);
    }

    const auto& stats = board.navigator().stats();
    EXPECT_GT(stats.fieldLookups, 0u);
    EXPECT_LE(board.navigator().fieldsBuilt(), stats.fieldLookups)
        << "a field was built for every lookup instead of being shared";
}

TEST(Routing, AirRequestsTakeTheStraightLine) {
    Board board;
    auto dragon = spawn(cards::Dragon, 10, 6, false);
    board.addEntity(dragon);
    board.addEntity(spawn(cards::KingTower, 19, 27, true));

    for (int i = 0; i < 100; ++i) {
        dragon->update(board, 0.1f);
    }

    const auto& stats = board.navigator().stats();
    EXPECT_GT(stats.directLines, 0u) << "an air unit was routed by search instead of a straight line";
}

TEST(Routing, AUnitWithNoReachableTargetSimplyHoldsStill) {
    // Sealed into a pocket, with its target outside it. The walls form a
    // complete ring around the interior x in [2,4], y in [4,6].
    Arena arena{20, 20};
    for (int x = 1; x <= 5; ++x) {
        arena.setTile(x, 3, Tile::Blocked);
        arena.setTile(x, 7, Tile::Blocked);
    }
    for (int y = 3; y <= 7; ++y) {
        arena.setTile(1, y, Tile::Blocked);
        arena.setTile(5, y, Tile::Blocked);
    }

    Board board{arena};
    auto knight = createEntity(defaultCards().get(cards::Knight), arena, 3, 5, true, Lane::LEFT);
    board.addEntity(knight);
    board.addEntity(createEntity(defaultCards().get(cards::QueenTower), arena, 15, 15, false, Lane::LEFT));

    for (int i = 0; i < 200; ++i) {
        board.updateEntities(0.1f);
        EXPECT_NE(arena.tile(knight->getX(), knight->getY()), Tile::Blocked);
    }
    // It stays inside the pocket rather than escaping or misbehaving.
    EXPECT_GE(knight->getX(), 2);
    EXPECT_LE(knight->getX(), 4);
}

// ---------------------------------------------------------------------------
// Targeting
// ---------------------------------------------------------------------------

TEST(Targeting, GroundUnitsIgnoreFlyingEnemiesEntirely) {
    // A Knight cannot attack air, so a Dragon is not even a movement target.
    auto knight = spawn(cards::Knight, 10, 10, true);
    Board board = boardWith(knight, cards::Dragon, 10, 20);

    tick(knight, board, 100);

    EXPECT_EQ(knight->getY(), 10) << "knight should not chase a target it cannot attack";
}

TEST(Targeting, GolemWalksPastACloserTroopToReachATower) {
    Board board;
    auto golem = spawn(cards::Golem, 10, 10, true);
    board.addEntity(golem);
    // A decoy one tile *behind* the Golem, and a tower ahead of it. A
    // nearest-target unit would turn around; the Golem must not.
    board.addEntity(spawn(cards::Knight, 10, 9, false));
    board.addEntity(spawn(cards::QueenTower, 10, 25, false));

    tick(golem, board, 20);

    EXPECT_EQ(golem->getY(), 11) << "golem moved toward the decoy instead of the tower";
}

// ---------------------------------------------------------------------------
// Board bookkeeping
// ---------------------------------------------------------------------------

TEST(BoardBookkeeping, RemovesDeadTroopsButRetainsDeadTowers) {
    Board board;
    auto troop = spawn(cards::Knight, 10, 10, true);
    auto tower = spawn(cards::QueenTower, 20, 20, false);
    board.addEntity(troop);
    board.addEntity(tower);

    troop->takeDamage(troop->getHealth());
    tower->takeDamage(tower->getHealth());
    ASSERT_FALSE(troop->isAlive());
    ASSERT_FALSE(tower->isAlive());

    board.updateEntities();

    ASSERT_EQ(board.getEntities().size(), 1u);
    EXPECT_EQ(board.getEntities().front()->cardId(), cards::QueenTower)
        << "dead towers are deliberately retained so win conditions can inspect them";
}

TEST(BoardBookkeeping, NegativeDamageIsRejectedAndHealthFloorsAtZero) {
    auto knight = spawn(cards::Knight, 10, 10, true);
    const int initial = knight->getHealth();

    knight->takeDamage(-100);
    EXPECT_EQ(knight->getHealth(), initial);

    knight->takeDamage(initial + 5000);
    EXPECT_EQ(knight->getHealth(), 0);
}

// ---------------------------------------------------------------------------
// Combat
// ---------------------------------------------------------------------------

namespace {

// A board with critical hits switched off, so damage is a single exact number.
// Under the old global rand() this was impossible: every attack rolled twice
// and the sequence differed between standard libraries, so tests could only
// assert ranges or sets of possible values.
Board deterministicBoard() {
    Board board;
    board.setCombatRules(CombatRules{/*criticalChance=*/0.0f, /*criticalMultiplier=*/1.5f});
    return board;
}

}  // namespace

// Entities attack on a cooldown derived from their attack speed. Previously
// there was none: handleCombat() landed a full hit for every in-range entity on
// every 0.1s tick, so a Knight dealt 500 damage per second instead of ~42.
TEST(Combat, AttacksAreRateLimitedByAttackSpeed) {
    Board board = deterministicBoard();
    auto knight = spawn(cards::Knight, 10, 24, true);
    auto tower = spawn(cards::QueenTower, 10, 25, false);
    board.addEntity(knight);
    board.addEntity(tower);

    Rng rng{1};
    const int initial = tower->getHealth();

    // A Knight hits every 1.2s. Five 0.1s ticks is 0.5s, so the opening hit
    // lands and nothing else does.
    for (int i = 0; i < 5; ++i) {
        board.handleCombat(rng, 0.1f);
    }
    EXPECT_EQ(initial - tower->getHealth(), 50) << "expected exactly one hit inside the cooldown";

    // Carrying on past 1.2s total lets a second hit through.
    for (int i = 0; i < 9; ++i) {
        board.handleCombat(rng, 0.1f);
    }
    EXPECT_EQ(initial - tower->getHealth(), 100) << "cooldown did not expire on schedule";
}

TEST(Combat, FirstAttackLandsImmediately) {
    // Units start ready, so engaging costs no warm-up.
    Board board = deterministicBoard();
    auto pekka = spawn(cards::Pekka, 10, 24, true);
    auto tower = spawn(cards::QueenTower, 10, 25, false);
    board.addEntity(pekka);
    board.addEntity(tower);

    Rng rng{1};
    const int initial = tower->getHealth();
    board.handleCombat(rng, 0.1f);

    EXPECT_LT(tower->getHealth(), initial);
}

TEST(Combat, DamagePerSecondIsIndependentOfTheTimestep) {
    // The cooldown is measured in seconds, so running at a coarser step must
    // not change how much damage lands over the same elapsed time.
    auto damageOverTenSeconds = [](float dt, int steps) {
        Board board = deterministicBoard();
        auto archers = spawn(cards::Archers, 10, 20, true);
        auto tower = spawn(cards::QueenTower, 10, 25, false);
        board.addEntity(archers);
        board.addEntity(tower);

        Rng rng{1};
        const int initial = tower->getHealth();
        for (int i = 0; i < steps; ++i) {
            board.handleCombat(rng, dt);
        }
        return initial - tower->getHealth();
    };

    // Archers hit every 1.2s, so ~9 hits land in 10 seconds either way.
    EXPECT_EQ(damageOverTenSeconds(0.1f, 100), damageOverTenSeconds(0.5f, 20));
}

TEST(Combat, CriticalHitAppliesExactlyOnePerAttack) {
    // Bracketing the probability at 0 and 1 pins the multiplier exactly,
    // without relying on sampling. A Knight's 50 damage is unmodified against a
    // Queen Tower, so the only variable is the critical multiplier.
    struct Case {
        float chance;
        int expected;
    };
    for (const Case c : {Case{0.0f, 50}, Case{1.0f, 75}}) {
        SCOPED_TRACE("criticalChance = " + std::to_string(c.chance));

        Board board;
        board.setCombatRules(CombatRules{c.chance, 1.5f});
        auto knight = spawn(cards::Knight, 10, 24, true);
        auto tower = spawn(cards::QueenTower, 10, 25, false);
        board.addEntity(knight);
        board.addEntity(tower);

        Rng rng{1};
        const int initial = tower->getHealth();
        board.handleCombat(rng);

        // 112 here would mean the 2.25x double-critical has come back.
        EXPECT_EQ(initial - tower->getHealth(), c.expected);
    }
}

TEST(Combat, HeavyArmorReducesIncomingDamage) {
    Board board = deterministicBoard();
    auto pekka = spawn(cards::Pekka, 10, 10, true);
    auto archers = spawn(cards::Archers, 10, 12, false);
    board.addEntity(pekka);
    board.addEntity(archers);

    Rng rng{1};
    const int initial = pekka->getHealth();
    board.handleCombat(rng);

    // Archers deal 20; PEKKA's heavy armor scales that to floor(20 * 0.6) = 12.
    EXPECT_EQ(initial - pekka->getHealth(), 12);
}

TEST(Combat, SameSeedProducesIdenticalDamage) {
    // The whole point of owning the generator: a match is reproducible.
    auto runOnce = [](std::uint64_t seed) {
        Board board;
        board.setCombatRules(CombatRules{0.5f, 1.5f});  // crit often, to vary output
        auto wizard = spawn(cards::Wizard, 10, 24, true);
        auto tower = spawn(cards::QueenTower, 10, 25, false);
        board.addEntity(wizard);
        board.addEntity(tower);

        Rng rng{seed};
        std::vector<int> damage;
        for (int i = 0; i < 20; ++i) {
            const int before = tower->getHealth();
            board.handleCombat(rng);
            damage.push_back(before - tower->getHealth());
        }
        return damage;
    };

    EXPECT_EQ(runOnce(4242), runOnce(4242));
    EXPECT_NE(runOnce(4242), runOnce(99));
}

// ---------------------------------------------------------------------------
// Deployment geometry
// ---------------------------------------------------------------------------

TEST(DeploymentGeometry, TroopsSpawnAtTheLaneSpawnPoints) {
    MatchConfig config;
    config.deterministic = true;
    config.seed = 7;
    Simulation sim{config};
    const size_t towerCount = sim.board().getEntities().size();

    ASSERT_TRUE(sim.deploy(cards::Archers, Lane::RIGHT, /*isPlayerOne=*/false));
    ASSERT_GT(sim.board().getEntities().size(), towerCount);

    // spawnX is kArenaWidth/4 or kArenaWidth*3/4; spawnY for player two is
    // kArenaHeight/2 - 3, shifted two tiles back for a Canon.
    const auto& spawned = sim.board().getEntities()[towerCount];
    EXPECT_EQ(spawned->getX(), 30);
    EXPECT_EQ(spawned->getY(), 14);
    EXPECT_FALSE(spawned->getIsPlayer());
}
