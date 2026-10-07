// Tests for the headless match simulation.
//
// These replace the old TestableGame suite, which subclassed Game purely to
// reach five protected virtuals. That indirection is gone: the rules now live
// in Simulation, whose public interface is directly drivable, and none of this
// file links the terminal front-end.

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "clash_royale/ai/random_controller.hpp"
#include "clash_royale/sim/entity_factory.hpp"
#include "clash_royale/sim/simulation.hpp"

using namespace cr;

namespace {

/// A reproducible match with critical hits disabled, so damage is exact.
MatchConfig seededConfig(std::uint64_t seed = 1234) {
    MatchConfig config;
    config.deterministic = true;
    config.seed = seed;
    config.combat.criticalChance = 0.0f;
    return config;
}

std::shared_ptr<Entity> findKingTower(Simulation& sim, bool isPlayerOne) {
    for (const auto& entity : sim.board().getEntities()) {
        if (entity->getType() == EntityType::KING_TOWER && entity->getIsPlayer() == isPlayerOne) {
            return entity;
        }
    }
    return nullptr;
}

}  // namespace

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

TEST(SimulationSetup, StartsWithSixTowersAndTheConfiguredElixir) {
    Simulation sim{seededConfig()};

    EXPECT_EQ(sim.board().getEntities().size(), 6u);
    EXPECT_FLOAT_EQ(sim.elixir(true), 5.0f);
    EXPECT_FLOAT_EQ(sim.elixir(false), 5.0f);
    EXPECT_TRUE(sim.isRunning());
    EXPECT_EQ(sim.result(), MatchResult::IN_PROGRESS);
    EXPECT_FLOAT_EQ(sim.elapsed(), 0.0f);
}

TEST(SimulationSetup, MatchConfigIsHonoured) {
    MatchConfig config = seededConfig();
    config.startingElixir = 1.0f;
    config.maxElixir = 3.0f;
    config.matchDuration = 5.0f;

    Simulation sim{config};
    EXPECT_FLOAT_EQ(sim.elixir(true), 1.0f);

    for (int i = 0; i < 200; ++i) {
        sim.step();
    }
    EXPECT_LE(sim.elixir(true), 3.0f) << "elixir exceeded the configured cap";
    EXPECT_FALSE(sim.isRunning()) << "match outlasted its configured duration";
}

// ---------------------------------------------------------------------------
// Elixir economy
// ---------------------------------------------------------------------------

TEST(SimulationElixir, GeneratesOnePointPerIntervalAndCaps) {
    Simulation sim{seededConfig()};
    const float initial = sim.elixir(true);

    // One point every 2.8s, i.e. every 28 steps of the default 0.1s timestep.
    for (int i = 0; i < 30; ++i) {
        sim.step();
    }
    EXPECT_FLOAT_EQ(sim.elixir(true), initial + 1.0f);

    for (int i = 0; i < 300; ++i) {
        sim.step();
    }
    EXPECT_LE(sim.elixir(true), sim.config().maxElixir);
    EXPECT_LE(sim.elixir(false), sim.config().maxElixir);
}

TEST(SimulationElixir, TracksElapsedTimeRatherThanTheCallCount) {
    // The elixir economy must depend on how much time has passed, not on how
    // many times step() was called. The old implementation zeroed its timer and
    // granted at most one point per call, so a single coarse step lost elixir
    // that a sequence of fine steps would have earned.
    //
    // 30 seconds at 2.8s per point is 10 points. maxElixir is raised so the cap
    // does not mask the comparison, and 30s is deliberately not a multiple of
    // the interval, to stay clear of a float-accumulation boundary.
    auto elixirAfter30Seconds = [](float dt, int steps) {
        MatchConfig config = seededConfig();
        config.maxElixir = 100.0f;
        config.matchDuration = 1000.0f;
        Simulation sim{config};
        for (int i = 0; i < steps; ++i) {
            sim.step(dt);
        }
        return sim.elixir(true);
    };

    const float expected = 5.0f + 10.0f;
    EXPECT_FLOAT_EQ(elixirAfter30Seconds(0.1f, 300), expected);
    EXPECT_FLOAT_EQ(elixirAfter30Seconds(3.0f, 10), expected);
    EXPECT_FLOAT_EQ(elixirAfter30Seconds(30.0f, 1), expected) << "a single large step lost elixir";
}

TEST(SimulationElixir, MovementAlsoHonoursTheTimestep) {
    // step() must forward dt to the entities, not just to its own clocks.
    // A Knight moves once per second, so one step(1.0f) and ten step(0.1f)
    // must advance it the same distance.
    auto knightPositionAfterOneSecond = [](float dt, int steps) {
        MatchConfig config = seededConfig();
        Simulation sim{config};
        sim.deploy(EntityType::KNIGHT, Lane::LEFT, true);
        auto knight = sim.board().getEntities().back();
        const int startY = knight->getY();
        for (int i = 0; i < steps; ++i) {
            sim.step(dt);
        }
        return startY - knight->getY();
    };

    const int fine = knightPositionAfterOneSecond(0.1f, 10);
    EXPECT_GT(fine, 0) << "the knight never advanced";
    EXPECT_EQ(knightPositionAfterOneSecond(1.0f, 1), fine) << "dt was not forwarded to entity movement";
}

// ---------------------------------------------------------------------------
// Deployment
// ---------------------------------------------------------------------------

TEST(SimulationDeploy, SpendsElixirAndPlacesTheUnit) {
    Simulation sim{seededConfig()};
    const size_t before = sim.board().getEntities().size();

    ASSERT_TRUE(sim.deploy(EntityType::ARCHERS, Lane::LEFT, true));

    EXPECT_EQ(sim.board().getEntities().size(), before + 1);
    EXPECT_FLOAT_EQ(sim.elixir(true), 5.0f - elixirCost(EntityType::ARCHERS));
    EXPECT_FLOAT_EQ(sim.elixir(false), 5.0f) << "deploying must not touch the opponent's elixir";
}

TEST(SimulationDeploy, RefusesWhatThePlayerCannotAfford) {
    Simulation sim{seededConfig()};
    // A Golem costs 5, so two of them cannot both be afforded from 5 elixir.
    ASSERT_TRUE(sim.deploy(EntityType::GOLEM, Lane::LEFT, true));
    const size_t after = sim.board().getEntities().size();

    EXPECT_FALSE(sim.deploy(EntityType::GOLEM, Lane::LEFT, true));
    EXPECT_EQ(sim.board().getEntities().size(), after) << "a refused deploy must change nothing";
    EXPECT_FLOAT_EQ(sim.elixir(true), 0.0f);
}

TEST(SimulationDeploy, PlacesUnitsAtTheLaneSpawnPoints) {
    Simulation sim{seededConfig()};

    ASSERT_TRUE(sim.deploy(EntityType::KNIGHT, Lane::LEFT, true));
    const auto& knight = sim.board().getEntities().back();
    EXPECT_EQ(knight->getX(), kArenaWidth / 4);
    EXPECT_EQ(knight->getY(), (kArenaHeight / 2) + 3);

    Simulation other{seededConfig()};
    ASSERT_TRUE(other.deploy(EntityType::CANON, Lane::RIGHT, false));
    const auto& canon = other.board().getEntities().back();
    EXPECT_EQ(canon->getX(), kArenaWidth * 3 / 4);
    // Buildings are placed two tiles behind the troop spawn line.
    EXPECT_EQ(canon->getY(), (kArenaHeight / 2) - 3 + 2);
}

TEST(SimulationDeploy, ElixirCostsAreSingleSourced) {
    // These were previously spread across three switch statements that could
    // disagree. Spot-check that the one remaining table is the one in use.
    for (EntityType type : {EntityType::ARCHERS, EntityType::GOBLINS, EntityType::KNIGHT, EntityType::GOLEM}) {
        Simulation fresh{seededConfig()};
        const float before = fresh.elixir(true);
        ASSERT_TRUE(fresh.deploy(type, Lane::LEFT, true));
        EXPECT_FLOAT_EQ(before - fresh.elixir(true), elixirCost(type));
    }
}

// ---------------------------------------------------------------------------
// Win conditions
// ---------------------------------------------------------------------------

// This is the regression test that could not be written before Simulation
// existed. Game::checkWinCondition() was correct but was never called from
// Game::update(), so destroying a King Tower did not end a real match -- play
// continued until the clock expired. The old suite missed it because it invoked
// checkWinCondition() by hand. The check now runs inside step().
TEST(SimulationWinCondition, DestroyingTheKingTowerEndsTheMatchFromStepAlone) {
    Simulation sim{seededConfig()};

    auto enemyKing = findKingTower(sim, /*isPlayerOne=*/false);
    ASSERT_NE(enemyKing, nullptr);
    enemyKing->takeDamage(enemyKing->getHealth() - 10);

    // A PEKKA adjacent to the weakened tower finishes it in a single hit.
    sim.board().addEntity(
        EntityFactory::create(EntityType::PEKKA, enemyKing->getX(), enemyKing->getY() + 1, true, Lane::LEFT));

    ASSERT_TRUE(sim.isRunning());
    sim.step();

    EXPECT_FALSE(enemyKing->isAlive());
    EXPECT_FALSE(sim.isRunning()) << "step() did not apply the win condition";
    EXPECT_EQ(sim.result(), MatchResult::PLAYER_ONE_WINS);
}

TEST(SimulationWinCondition, LosingYourOwnKingTowerLosesTheMatch) {
    Simulation sim{seededConfig()};

    auto ownKing = findKingTower(sim, /*isPlayerOne=*/true);
    ASSERT_NE(ownKing, nullptr);
    ownKing->takeDamage(ownKing->getHealth());

    sim.step();

    EXPECT_EQ(sim.result(), MatchResult::PLAYER_TWO_WINS);
}

TEST(SimulationWinCondition, RunningOutOfTimeIsDecidedOnTowerHealth) {
    MatchConfig config = seededConfig();
    config.matchDuration = 1.0f;
    Simulation sim{config};

    // Chip a point off one of player two's towers so the sides are unequal.
    auto enemyKing = findKingTower(sim, /*isPlayerOne=*/false);
    ASSERT_NE(enemyKing, nullptr);
    enemyKing->takeDamage(500);
    ASSERT_GT(sim.towerHealth(true), sim.towerHealth(false));

    for (int i = 0; i < 20 && sim.isRunning(); ++i) {
        sim.step();
    }

    ASSERT_FALSE(sim.isRunning());
    EXPECT_EQ(sim.result(), MatchResult::PLAYER_ONE_WINS);
}

TEST(SimulationWinCondition, UntouchedTowersAtFullTimeAreADraw) {
    MatchConfig config = seededConfig();
    config.matchDuration = 1.0f;
    Simulation sim{config};

    for (int i = 0; i < 20 && sim.isRunning(); ++i) {
        sim.step();
    }

    ASSERT_FALSE(sim.isRunning());
    EXPECT_EQ(sim.result(), MatchResult::DRAW);
}

TEST(SimulationWinCondition, SteppingAFinishedMatchIsANoOp) {
    MatchConfig config = seededConfig();
    config.matchDuration = 0.5f;
    Simulation sim{config};

    while (sim.isRunning()) {
        sim.step();
    }
    const float ended = sim.elapsed();
    const MatchResult result = sim.result();

    sim.step();
    sim.step();

    EXPECT_FLOAT_EQ(sim.elapsed(), ended) << "the clock kept running after the match ended";
    EXPECT_EQ(sim.result(), result);
}

// ---------------------------------------------------------------------------
// AI controller
// ---------------------------------------------------------------------------

TEST(RandomAi, DeploysOnceItCanAffordSomething) {
    Simulation sim{seededConfig()};
    RandomAiController ai{/*deployChancePerStep=*/1.0f, /*minimumElixir=*/3.0f};

    const size_t before = sim.board().getEntities().size();
    ai.update(sim, /*isPlayerOne=*/false, kDefaultTimeStep);

    EXPECT_GT(sim.board().getEntities().size(), before);
    EXPECT_LT(sim.elixir(false), 5.0f) << "a deploy should have cost elixir";
    EXPECT_FLOAT_EQ(sim.elixir(true), 5.0f) << "the AI spent the player's elixir";
}

TEST(RandomAi, HoldsBelowItsMinimumElixir) {
    Simulation sim{seededConfig()};
    RandomAiController ai{1.0f, /*minimumElixir=*/6.0f};  // above the 5.0 start

    const size_t before = sim.board().getEntities().size();
    for (int i = 0; i < 10; ++i) {
        ai.update(sim, false, kDefaultTimeStep);
    }

    EXPECT_EQ(sim.board().getEntities().size(), before);
}

TEST(RandomAi, NeverDeploysWithAZeroChance) {
    Simulation sim{seededConfig()};
    RandomAiController ai{/*deployChancePerStep=*/0.0f};

    const size_t before = sim.board().getEntities().size();
    for (int i = 0; i < 100; ++i) {
        ai.update(sim, false, kDefaultTimeStep);
    }

    EXPECT_EQ(sim.board().getEntities().size(), before);
}

TEST(RandomAi, OnlyEverDeploysUnitsItCanAfford) {
    Simulation sim{seededConfig()};
    RandomAiController ai{1.0f, 0.0f};

    for (int i = 0; i < 500; ++i) {
        ai.update(sim, false, kDefaultTimeStep);
        sim.step();
        ASSERT_GE(sim.elixir(false), 0.0f) << "AI overdrew its elixir on step " << i;
    }
}

// ---------------------------------------------------------------------------
// Reproducibility
// ---------------------------------------------------------------------------

TEST(SimulationDeterminism, SameSeedReplaysIdentically) {
    auto playMatch = [](std::uint64_t seed) {
        MatchConfig config;
        config.deterministic = true;
        config.seed = seed;
        config.matchDuration = 30.0f;

        Simulation sim{config};
        RandomAiController ai;
        while (sim.isRunning()) {
            ai.update(sim, false, kDefaultTimeStep);
            sim.step();
        }

        // Fingerprint the final state rather than just the winner, so a
        // divergence anywhere in the match is caught.
        std::vector<int> fingerprint{static_cast<int>(sim.result()), sim.towerHealth(true), sim.towerHealth(false),
                                     static_cast<int>(sim.board().getEntities().size())};
        for (const auto& entity : sim.board().getEntities()) {
            fingerprint.push_back(entity->getX());
            fingerprint.push_back(entity->getY());
            fingerprint.push_back(entity->getHealth());
        }
        return fingerprint;
    };

    EXPECT_EQ(playMatch(2024), playMatch(2024)) << "a seeded match is not reproducible";
    EXPECT_NE(playMatch(2024), playMatch(777)) << "different seeds produced identical matches";
}

TEST(SimulationDeterminism, SimulationsDoNotPerturbEachOther) {
    // Two matches interleaved in one process. With global rand() this was
    // impossible: each draw advanced shared state.
    MatchConfig config = seededConfig(99);
    config.matchDuration = 10.0f;

    Simulation alone{config};
    RandomAiController aiAlone;
    while (alone.isRunning()) {
        aiAlone.update(alone, false, kDefaultTimeStep);
        alone.step();
    }

    Simulation interleaved{config};
    Simulation noise{seededConfig(5)};
    RandomAiController aiInterleaved;
    RandomAiController aiNoise;
    while (interleaved.isRunning()) {
        aiInterleaved.update(interleaved, false, kDefaultTimeStep);
        interleaved.step();
        if (noise.isRunning()) {
            aiNoise.update(noise, false, kDefaultTimeStep);
            noise.step();
        }
    }

    EXPECT_EQ(interleaved.towerHealth(true), alone.towerHealth(true));
    EXPECT_EQ(interleaved.towerHealth(false), alone.towerHealth(false));
    EXPECT_EQ(interleaved.result(), alone.result());
}

TEST(SimulationDeterminism, EntropySeedIsRecoverableForReplay) {
    Simulation sim;  // non-deterministic by default
    const std::uint64_t used = sim.rng().seed();

    MatchConfig replayed;
    replayed.deterministic = true;
    replayed.seed = used;

    Simulation replay{replayed};
    EXPECT_EQ(replay.rng().seed(), used);
}
