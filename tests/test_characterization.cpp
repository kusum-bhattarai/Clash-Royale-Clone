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

#include "clash_royale/core/rng.hpp"
#include "clash_royale/core/types.hpp"
#include "clash_royale/sim/entity_factory.hpp"
#include "clash_royale/sim/board.hpp"
#include "clash_royale/sim/simulation.hpp"
#include "clash_royale/tui/renderer.hpp"
#include "clash_royale/sim/knight.hpp"
#include "clash_royale/sim/entity.hpp"

using namespace cr;

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
    EXPECT_EQ(kArenaWidth, 40);
    EXPECT_EQ(kArenaHeight, 35);
}

TEST(ArenaGeometry, MatchStartsWithSixTowersAtFixedPositions) {
    Simulation sim;
    const auto& entities = sim.board().getEntities();

    ASSERT_EQ(entities.size(), 6u);

    struct Expected {
        EntityType type;
        int x, y;
        bool isPlayer;
    };
    // centerX = 19, sideOffset = 12. Note the left queen towers sit at x=6 and
    // the right at x=31 -- 13 and 12 tiles from center respectively. The arena
    // is NOT horizontally symmetric, due to the `- 1 -` in the left-tower
    // offset in Simulation's constructor.
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
    Knight tooLow(EntityType::KNIGHT, -5, -5, true, 600, Lane::LEFT);
    EXPECT_EQ(tooLow.getX(), 1);
    EXPECT_EQ(tooLow.getY(), 1);

    Knight tooHigh(EntityType::KNIGHT, 999, 999, true, 600, Lane::LEFT);
    EXPECT_EQ(tooHigh.getX(), kArenaWidth - 2);   // 38
    EXPECT_EQ(tooHigh.getY(), kArenaHeight - 2);  // 33
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

// Goblins alternate which axis they close on. This branch existed from the
// start but was unreachable: it keyed on `m_moveTimer`, which Entity::update()
// zeroes immediately before calling move(), so only the horizontal-first case
// ever ran. It now keys on the movement step count.
TEST(MovementShape, GoblinsAlternateAxesAsTheyClose) {
    auto goblins = EntityFactory::create(EntityType::GOBLINS, 10, 10, true, Lane::LEFT);
    Board board = boardWith(goblins, EntityType::KNIGHT, 20, 20);

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
    auto knight = EntityFactory::create(EntityType::KNIGHT, 10, 24, true, Lane::LEFT);
    auto tower = EntityFactory::create(EntityType::QUEEN_TOWER, 10, 25, false, Lane::LEFT);
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
    auto pekka = EntityFactory::create(EntityType::PEKKA, 10, 24, true, Lane::LEFT);
    auto tower = EntityFactory::create(EntityType::QUEEN_TOWER, 10, 25, false, Lane::LEFT);
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
        auto archers = EntityFactory::create(EntityType::ARCHERS, 10, 20, true, Lane::LEFT);
        auto tower = EntityFactory::create(EntityType::QUEEN_TOWER, 10, 25, false, Lane::LEFT);
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
        auto knight = EntityFactory::create(EntityType::KNIGHT, 10, 24, true, Lane::LEFT);
        auto tower = EntityFactory::create(EntityType::QUEEN_TOWER, 10, 25, false, Lane::LEFT);
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
    auto pekka = EntityFactory::create(EntityType::PEKKA, 10, 10, true, Lane::LEFT);
    auto archers = EntityFactory::create(EntityType::ARCHERS, 10, 12, false, Lane::LEFT);
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
        auto wizard = EntityFactory::create(EntityType::WIZARD, 10, 24, true, Lane::LEFT);
        auto tower = EntityFactory::create(EntityType::QUEEN_TOWER, 10, 25, false, Lane::LEFT);
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

    ASSERT_TRUE(sim.deploy(EntityType::ARCHERS, Lane::RIGHT, /*isPlayerOne=*/false));
    ASSERT_GT(sim.board().getEntities().size(), towerCount);

    // spawnX is kArenaWidth/4 or kArenaWidth*3/4; spawnY for player two is
    // kArenaHeight/2 - 3, shifted two tiles back for a Canon.
    const auto& spawned = sim.board().getEntities()[towerCount];
    EXPECT_EQ(spawned->getX(), 30);
    EXPECT_EQ(spawned->getY(), 14);
    EXPECT_FALSE(spawned->getIsPlayer());
}
