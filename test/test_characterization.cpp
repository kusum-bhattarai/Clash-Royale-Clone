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

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <string>

#include "core/EntityFactory.hpp"
#include "core/board.hpp"
#include "core/game.hpp"
#include "core/renderer.hpp"
#include "entity/Knight.hpp"
#include "entity/entity.hpp"

namespace {

// Drives an entity for `ticks` updates of the fixed 0.1s game step.
void tick(const std::shared_ptr<Entity>& entity, const Board& board, int ticks) {
    for (int i = 0; i < ticks; ++i) {
        entity->update(board);
    }
}

// Builds a board holding `self` plus a single enemy at (ex, ey), so movement
// can be observed against exactly one target with no ambiguity.
Board boardWith(const std::shared_ptr<Entity>& self, EntityType enemyType, int ex, int ey) {
    Board board;
    board.addEntity(self);
    board.addEntity(EntityFactory::create(enemyType, ex, ey, !self->getIsPlayer(), Lane::LEFT));
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

TEST(ArenaGeometry, BoardDimensions) {
    EXPECT_EQ(Renderer::BOARD_WIDTH, 40);
    EXPECT_EQ(Renderer::BOARD_HEIGHT, 35);
}

TEST(ArenaGeometry, GameStartsWithSixTowersAtFixedPositions) {
    Game game;
    const auto& entities = game.getBoard().getEntities();

    ASSERT_EQ(entities.size(), 6u);

    struct Expected {
        EntityType type;
        int x, y;
        bool isPlayer;
    };
    // centerX = 19, sideOffset = 12. Note the left queen towers sit at x=6 and
    // the right at x=31 -- 13 and 12 tiles from center respectively. The arena
    // is NOT horizontally symmetric, due to the `- 1 -` in the left-tower
    // offset in Game's constructor.
    const Expected expected[] = {
        {EntityType::KING_TOWER, 19, 3, false},
        {EntityType::QUEEN_TOWER, 6, 5, false},
        {EntityType::QUEEN_TOWER, 31, 5, false},
        {EntityType::KING_TOWER, 19, 27, true},
        {EntityType::QUEEN_TOWER, 6, 25, true},
        {EntityType::QUEEN_TOWER, 31, 25, true},
    };

    for (size_t i = 0; i < std::size(expected); ++i) {
        SCOPED_TRACE("tower index " + std::to_string(i));
        EXPECT_EQ(entities[i]->getType(), expected[i].type);
        EXPECT_EQ(entities[i]->getX(), expected[i].x);
        EXPECT_EQ(entities[i]->getY(), expected[i].y);
        EXPECT_EQ(entities[i]->getIsPlayer(), expected[i].isPlayer);
    }
}

TEST(ArenaGeometry, GameStartsWithFiveElixirEach) {
    Game game;
    EXPECT_FLOAT_EQ(game.getElixirPlayerOne(), 5.0f);
    EXPECT_FLOAT_EQ(game.getElixirPlayerTwo(), 5.0f);
    EXPECT_TRUE(game.getIsRunning());
}

// ---------------------------------------------------------------------------
// Entity construction invariants
// ---------------------------------------------------------------------------

TEST(EntityConstruction, ClampsPositionsOutsideTheArena) {
    Knight tooLow(EntityType::KNIGHT, -5, -5, true, 600, Lane::LEFT);
    EXPECT_EQ(tooLow.getX(), 1);
    EXPECT_EQ(tooLow.getY(), 1);

    Knight tooHigh(EntityType::KNIGHT, 999, 999, true, 600, Lane::LEFT);
    EXPECT_EQ(tooHigh.getX(), Renderer::BOARD_WIDTH - 2);   // 38
    EXPECT_EQ(tooHigh.getY(), Renderer::BOARD_HEIGHT - 2);  // 33
}

TEST(EntityConstruction, RaisesNonPositiveHealthToOne) {
    Knight dead(EntityType::KNIGHT, 10, 10, true, 0, Lane::LEFT);
    EXPECT_EQ(dead.getHealth(), 1);
    EXPECT_TRUE(dead.isAlive());
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
    EntityType type;
    int ticksPerStep;
};

class MovementCadence : public ::testing::TestWithParam<CadenceCase> {};

TEST_P(MovementCadence, StepsOnExpectedTick) {
    const CadenceCase param = GetParam();

    auto unit = EntityFactory::create(param.type, 10, 10, true, Lane::LEFT);
    // Must be a tower: the Golem ignores troops entirely, so a troop decoy
    // would leave it with no target and no movement at all.
    Board board = boardWith(unit, EntityType::QUEEN_TOWER, 10, 30);
    const int startY = unit->getY();

    tick(unit, board, param.ticksPerStep - 1);
    EXPECT_EQ(unit->getY(), startY) << "moved one tick too early";

    tick(unit, board, 1);
    EXPECT_NE(unit->getY(), startY) << "failed to move on the expected tick";
}

INSTANTIATE_TEST_SUITE_P(
    AllUnits, MovementCadence,
    ::testing::Values(CadenceCase{EntityType::GOLEM, 20},    // speed 0.50
                      CadenceCase{EntityType::PEKKA, 14},    // speed 0.75
                      CadenceCase{EntityType::KNIGHT, 10},   // speed 1.00
                      CadenceCase{EntityType::WIZARD, 10},   // speed 1.00
                      CadenceCase{EntityType::GOBLINS, 9},   // speed 1.20
                      CadenceCase{EntityType::ARCHERS, 9},   // speed 1.20
                      CadenceCase{EntityType::DRAGON, 7}));  // speed 1.50

TEST(MovementCadence, StationaryUnitsNeverStep) {
    for (EntityType type : {EntityType::KING_TOWER, EntityType::QUEEN_TOWER, EntityType::CANON}) {
        auto building = EntityFactory::create(type, 10, 10, true, Lane::LEFT);
        Board board = boardWith(building, EntityType::KNIGHT, 10, 30);

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
    auto knight = EntityFactory::create(EntityType::KNIGHT, 10, 10, true, Lane::LEFT);
    // dx == dy == 10, so |dx| > |dy| is false and the tie resolves vertically.
    Board board = boardWith(knight, EntityType::KNIGHT, 20, 20);

    tick(knight, board, 10);

    EXPECT_EQ(knight->getX(), 10);
    EXPECT_EQ(knight->getY(), 11);
}

TEST(MovementShape, GroundUnitsMoveHorizontallyWhenHorizontalDistanceDominates) {
    auto knight = EntityFactory::create(EntityType::KNIGHT, 10, 10, true, Lane::LEFT);
    Board board = boardWith(knight, EntityType::KNIGHT, 30, 12);

    tick(knight, board, 10);

    EXPECT_EQ(knight->getX(), 11);
    EXPECT_EQ(knight->getY(), 10);
}

TEST(MovementShape, DragonMovesDiagonallyOnBothAxesAtOnce) {
    auto dragon = EntityFactory::create(EntityType::DRAGON, 10, 10, true, Lane::LEFT);
    Board board = boardWith(dragon, EntityType::KNIGHT, 20, 20);

    tick(dragon, board, 7);

    EXPECT_EQ(dragon->getX(), 11);
    EXPECT_EQ(dragon->getY(), 11);
}

TEST(MovementShape, RangedUnitsHaltOnceTheTargetIsWithinAttackRange) {
    // Archers have range 7. Starting 10 tiles away, they close to exactly 7 and
    // then stop advancing.
    auto archers = EntityFactory::create(EntityType::ARCHERS, 10, 10, true, Lane::LEFT);
    Board board = boardWith(archers, EntityType::KNIGHT, 10, 20);

    tick(archers, board, 200);

    EXPECT_EQ(archers->getX(), 10);
    EXPECT_EQ(archers->getY(), 13) << "archers should stop at exactly their attack range";
}

TEST(MovementShape, UnitsWithNoEnemyOnTheBoardDoNotMove) {
    Board board;
    auto knight = EntityFactory::create(EntityType::KNIGHT, 10, 10, true, Lane::LEFT);
    board.addEntity(knight);
    board.addEntity(EntityFactory::create(EntityType::KNIGHT, 10, 20, true, Lane::LEFT));

    tick(knight, board, 100);

    EXPECT_EQ(knight->getX(), 10);
    EXPECT_EQ(knight->getY(), 10);
}

// `Goblins::move` contains a zigzag branch keyed on `m_moveTimer`, but
// `Entity::update` zeroes `m_moveTimer` on the line *before* it calls `move()`.
// The timer is therefore always 0.0 inside `move()`, `movePattern` is always 0,
// and the even/horizontal-first branch is the only one that has ever run. The
// odd/vertical-first branch is unreachable dead code.
//
// This pins the behavior that actually occurs, not the behavior that was
// intended. Phase 5 decides whether to restore a real zigzag or drop it.
TEST(MovementShape, Bug_GoblinZigzagIsUnreachableAndAlwaysMovesHorizontallyFirst) {
    auto goblins = EntityFactory::create(EntityType::GOBLINS, 10, 10, true, Lane::LEFT);
    Board board = boardWith(goblins, EntityType::KNIGHT, 20, 20);

    // A true zigzag would alternate: (11,10) then (11,11) then (12,11)...
    // Instead every step that has a horizontal component goes horizontal.
    tick(goblins, board, 9);
    EXPECT_EQ(goblins->getX(), 11);
    EXPECT_EQ(goblins->getY(), 10);

    tick(goblins, board, 9);
    EXPECT_EQ(goblins->getX(), 12) << "second step also went horizontal; no alternation";
    EXPECT_EQ(goblins->getY(), 10);
}

// ---------------------------------------------------------------------------
// Targeting
// ---------------------------------------------------------------------------

TEST(Targeting, GroundUnitsIgnoreFlyingEnemiesEntirely) {
    // A Knight cannot attack air, so a Dragon is not even a movement target.
    auto knight = EntityFactory::create(EntityType::KNIGHT, 10, 10, true, Lane::LEFT);
    Board board = boardWith(knight, EntityType::DRAGON, 10, 20);

    tick(knight, board, 100);

    EXPECT_EQ(knight->getY(), 10) << "knight should not chase a target it cannot attack";
}

TEST(Targeting, GolemWalksPastACloserTroopToReachATower) {
    Board board;
    auto golem = EntityFactory::create(EntityType::GOLEM, 10, 10, true, Lane::LEFT);
    board.addEntity(golem);
    // A decoy one tile *behind* the Golem, and a tower ahead of it. A
    // nearest-target unit would turn around; the Golem must not.
    board.addEntity(EntityFactory::create(EntityType::KNIGHT, 10, 9, false, Lane::LEFT));
    board.addEntity(EntityFactory::create(EntityType::QUEEN_TOWER, 10, 25, false, Lane::LEFT));

    tick(golem, board, 20);

    EXPECT_EQ(golem->getY(), 11) << "golem moved toward the decoy instead of the tower";
}

// ---------------------------------------------------------------------------
// Board bookkeeping
// ---------------------------------------------------------------------------

TEST(BoardBookkeeping, RemovesDeadTroopsButRetainsDeadTowers) {
    Board board;
    auto troop = EntityFactory::create(EntityType::KNIGHT, 10, 10, true, Lane::LEFT);
    auto tower = EntityFactory::create(EntityType::QUEEN_TOWER, 20, 20, false, Lane::LEFT);
    board.addEntity(troop);
    board.addEntity(tower);

    troop->takeDamage(troop->getHealth());
    tower->takeDamage(tower->getHealth());
    ASSERT_FALSE(troop->isAlive());
    ASSERT_FALSE(tower->isAlive());

    board.updateEntities();

    ASSERT_EQ(board.getEntities().size(), 1u);
    EXPECT_EQ(board.getEntities().front()->getType(), EntityType::QUEEN_TOWER)
        << "dead towers are deliberately retained so win conditions can inspect them";
}

TEST(BoardBookkeeping, NegativeDamageIsRejectedAndHealthFloorsAtZero) {
    auto knight = EntityFactory::create(EntityType::KNIGHT, 10, 10, true, Lane::LEFT);
    const int initial = knight->getHealth();

    knight->takeDamage(-100);
    EXPECT_EQ(knight->getHealth(), initial);

    knight->takeDamage(initial + 5000);
    EXPECT_EQ(knight->getHealth(), 0);
}

// ---------------------------------------------------------------------------
// Combat
// ---------------------------------------------------------------------------

// There is no attack cooldown: `handleCombat` is called once per 0.1s tick and
// every in-range entity lands a full hit every single time. A Knight (50 damage)
// therefore deals 500 damage per second. Phase 2 introduces an `attackSpeed`
// stat, after which this test must be rewritten to expect ~1 hit per interval.
TEST(Combat, Bug_EveryEntityAttacksOnEveryTickWithNoCooldown) {
    Board board;
    auto knight = EntityFactory::create(EntityType::KNIGHT, 10, 24, true, Lane::LEFT);
    auto tower = EntityFactory::create(EntityType::QUEEN_TOWER, 10, 25, false, Lane::LEFT);
    board.addEntity(knight);
    board.addEntity(tower);

    const int initial = tower->getHealth();
    const int kTicks = 5;
    for (int i = 0; i < kTicks; ++i) {
        board.handleCombat();
    }

    // A Knight's 50 damage is unmodified against a Queen Tower, and critical
    // hits can only increase it -- so 5 ticks of combat must remove at least
    // 5 x 50 HP if and only if every tick produced an attack.
    const int taken = initial - tower->getHealth();
    EXPECT_GE(taken, kTicks * 50) << "expected one full hit per tick";
}

// `calculateDamage` rolls a 5% critical hit twice (board.cpp:76 and board.cpp:100),
// each applying a 1.5x multiplier. A double critical therefore yields 2.25x, and
// the effective crit rate is ~9.75% rather than the intended 5%. Phase 2 removes
// the duplicate roll.
//
// Seeded so the sequence is fixed for a given run; the trial count is set high
// enough that observing at least one 2.25x hit is overwhelmingly likely on any
// rand() implementation (p(miss) is on the order of 1e-6).
TEST(Combat, Bug_CriticalHitIsRolledTwicePerAttack) {
    std::srand(12345);

    const int kTrials = 5000;
    int maxObserved = 0;
    for (int i = 0; i < kTrials; ++i) {
        Board board;
        auto knight = EntityFactory::create(EntityType::KNIGHT, 10, 24, true, Lane::LEFT);
        auto tower = EntityFactory::create(EntityType::QUEEN_TOWER, 10, 25, false, Lane::LEFT);
        board.addEntity(knight);
        board.addEntity(tower);

        const int initial = tower->getHealth();
        board.handleCombat();
        maxObserved = std::max(maxObserved, initial - tower->getHealth());
    }

    // Base 50; single crit floors to 75; double crit floors to 112.
    EXPECT_EQ(maxObserved, 112) << "expected a reachable 2.25x double-critical hit";
}

TEST(Combat, HeavyArmorReducesIncomingDamage) {
    Board board;
    auto pekka = EntityFactory::create(EntityType::PEKKA, 10, 10, true, Lane::LEFT);
    auto archers = EntityFactory::create(EntityType::ARCHERS, 10, 12, false, Lane::LEFT);
    board.addEntity(pekka);
    board.addEntity(archers);

    const int initial = pekka->getHealth();
    board.handleCombat();

    // Archers deal 20; PEKKA's heavy armor scales that to floor(20 * 0.6) = 12.
    // The two independent critical rolls make 18 and 27 reachable as well, so
    // the full set of outcomes is pinned rather than a single value.
    const int taken = initial - pekka->getHealth();
    EXPECT_TRUE(taken == 12 || taken == 18 || taken == 27)
        << "unexpected damage " << taken << "; heavy armor scaling may have changed";
}

// ---------------------------------------------------------------------------
// Game loop wiring
// ---------------------------------------------------------------------------

namespace {

class SpawnProbeGame : public Game {
public:
    using Game::runAI;
};

}  // namespace

TEST(GameLoop, AiTroopsSpawnAtTheLaneSpawnPoints) {
    SpawnProbeGame game;
    // Seed *after* construction: Game's constructor calls srand(time(nullptr)),
    // which would otherwise discard the seed. (Phase 2 replaces global rand()
    // with an injectable, seedable generator owned by the simulation.)
    std::srand(7);
    Board& board = game.getBoard();
    const size_t towerCount = board.getEntities().size();

    for (int i = 0; i < 2000 && board.getEntities().size() == towerCount; ++i) {
        game.runAI();
    }
    ASSERT_GT(board.getEntities().size(), towerCount) << "AI never deployed";

    // spawnX is BOARD_WIDTH/4 or BOARD_WIDTH*3/4; spawnY for the AI is
    // BOARD_HEIGHT/2 - 3, shifted two tiles back for a Canon.
    const auto& spawned = board.getEntities()[towerCount];
    EXPECT_TRUE(spawned->getX() == 10 || spawned->getX() == 30) << "x = " << spawned->getX();
    const int expectedY = (spawned->getType() == EntityType::CANON) ? 16 : 14;
    EXPECT_EQ(spawned->getY(), expectedY);
    EXPECT_FALSE(spawned->getIsPlayer());
}

// NOTE (no test yet): `Game::checkWinCondition()` correctly detects a destroyed
// King Tower, but `Game::update()` never calls it -- update() only checks the
// 120s timer. Destroying a King Tower therefore does NOT end a real game. This
// cannot be pinned from a test today because `update()` is private and `run()`
// drives the terminal; Phase 1 makes the loop body reachable and Phase 2 adds
// the regression test alongside the fix.
