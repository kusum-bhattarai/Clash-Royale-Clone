// Tests for the strategic opponent.
//
// Two things matter beyond "it wins more often". First, that its reasoning comes
// from CardSpec data rather than from knowing which cards exist -- an AI that
// named the built-in roster would be useless to anyone who added their own.
// Second, that it keeps an economy: the first version answered every threat on
// every step and never attacked at all.

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "clash_royale/ai/random_controller.hpp"
#include "clash_royale/ai/strategic_controller.hpp"
#include "clash_royale/sim/simulation.hpp"

#include "test_helpers.hpp"

using namespace cr;
using namespace cr::testing;

namespace {

/// Tuning for observing *what* the controller picks: it acts at once, and the
/// sufficiency check is disabled.
///
/// That check is otherwise load-bearing enough to mask these tests. Against a
/// lone threat near a tower the controller correctly concludes the tower's own
/// guns are enough and deploys nothing, which is good play but says nothing
/// about its choice of counter.
StrategicAiController::Tuning promptTuning() {
    StrategicAiController::Tuning tuning;
    tuning.defenceInterval = 0.0f;
    tuning.pushInterval = 0.0f;
    tuning.defenceSufficiency = 1000.0f;  // always consider the threat unanswered
    return tuning;
}

/// Tuning that acts at once but keeps the real sufficiency judgement.
StrategicAiController::Tuning promptButThriftyTuning() {
    StrategicAiController::Tuning tuning;
    tuning.defenceInterval = 0.0f;
    tuning.pushInterval = 0.0f;
    return tuning;
}

/// Drops an enemy unit close enough to player one's towers to be a threat.
std::shared_ptr<Entity> placeThreat(Simulation& sim, std::string_view cardId) {
    return sim.board().spawn(sim.cards().get(cardId), 20, 20, /*isPlayer=*/false, Lane::LEFT);
}

/// The most recently added entity, i.e. whatever the controller just deployed.
const Entity* lastDeployed(const Simulation& sim, std::size_t before) {
    const auto& entities = sim.board().getEntities();
    return entities.size() > before ? entities.back().get() : nullptr;
}

MatchConfig seeded(std::uint64_t seed = 1) {
    MatchConfig config;
    config.deterministic = true;
    config.seed = seed;
    return config;
}

}  // namespace

// ---------------------------------------------------------------------------
// Counter selection
// ---------------------------------------------------------------------------

TEST(StrategicAi, AnswersAFlierWithSomethingThatCanHitAir) {
    Simulation sim{seeded()};
    placeThreat(sim, cards::Dragon);

    StrategicAiController ai{promptTuning()};
    const std::size_t before = sim.board().getEntities().size();
    ai.update(sim, /*isPlayerOne=*/true, kDefaultTimeStep);

    const Entity* deployed = lastDeployed(sim, before);
    ASSERT_NE(deployed, nullptr) << "nothing was deployed against an air threat";
    EXPECT_TRUE(deployed->spec().targets.air)
        << "deployed " << deployed->cardId() << ", which cannot attack air at all";
    EXPECT_EQ(ai.telemetry().defensiveDeploys, 1u);
}

TEST(StrategicAi, NeverDefendsWithACardThatIgnoresTroops) {
    // A buildings-only card walks straight past a push, so however strong it is
    // it is not a defender.
    Simulation sim{seeded()};
    placeThreat(sim, cards::Knight);

    StrategicAiController ai{promptTuning()};
    const std::size_t before = sim.board().getEntities().size();
    ai.update(sim, true, kDefaultTimeStep);

    const Entity* deployed = lastDeployed(sim, before);
    ASSERT_NE(deployed, nullptr);
    EXPECT_FALSE(deployed->spec().targets.buildingsOnly) << "deployed " << deployed->cardId() << " to defend";
}

TEST(StrategicAi, DefendsInTheLaneUnderThreat) {
    for (const auto& [threatX, expectedLane] : {std::pair{10, Lane::LEFT}, std::pair{30, Lane::RIGHT}}) {
        Simulation sim{seeded()};
        sim.board().spawn(sim.cards().get(cards::Knight), threatX, 20, false, Lane::LEFT);

        StrategicAiController ai{promptTuning()};
        const std::size_t before = sim.board().getEntities().size();
        ai.update(sim, true, kDefaultTimeStep);

        const Entity* deployed = lastDeployed(sim, before);
        ASSERT_NE(deployed, nullptr);
        EXPECT_EQ(deployed->getLane(), expectedLane) << "answered a threat at x=" << threatX << " in the wrong lane";
    }
}

// ---------------------------------------------------------------------------
// Economy
// ---------------------------------------------------------------------------

TEST(StrategicAi, LeavesALoneThreatToTheTowersOwnGuns) {
    // The sufficiency judgement in its own right: a single weak unit walking
    // into a King Tower's field of fire is not worth answering with elixir.
    Simulation sim{seeded()};
    placeThreat(sim, cards::Goblins);

    StrategicAiController ai{promptButThriftyTuning()};
    const std::size_t before = sim.board().getEntities().size();
    for (int i = 0; i < 20; ++i) {
        ai.update(sim, true, kDefaultTimeStep);
    }

    EXPECT_EQ(sim.board().getEntities().size(), before) << "spent elixir on a threat the towers cover";
    EXPECT_FLOAT_EQ(sim.elixir(true), 5.0f);
}

TEST(StrategicAi, HoldsElixirWhenThereIsNothingToAnswer) {
    // An empty board means no threats, and the starting 5 elixir is below the
    // push threshold, so the right move is to wait.
    Simulation sim{seeded()};
    StrategicAiController ai;

    const std::size_t before = sim.board().getEntities().size();
    for (int i = 0; i < 10; ++i) {
        ai.update(sim, true, kDefaultTimeStep);
    }

    EXPECT_EQ(sim.board().getEntities().size(), before) << "deployed with nothing to answer and no push banked";
    EXPECT_FLOAT_EQ(sim.elixir(true), 5.0f);
    EXPECT_EQ(ai.telemetry().stepsHeld, 10u);
}

TEST(StrategicAi, AttacksOnceItHasBankedEnough) {
    Simulation sim{seeded()};
    StrategicAiController ai;

    for (int i = 0; i < 400 && ai.telemetry().offensiveDeploys == 0; ++i) {
        ai.update(sim, true, kDefaultTimeStep);
        sim.step();
    }

    EXPECT_GT(ai.telemetry().offensiveDeploys, 0u) << "never attacked despite a full elixir bar";
}

TEST(StrategicAi, DoesNotStackDefendersOnACoveredThreat) {
    // A lone Goblin walking at a tower that is already covered by that tower's
    // own guns plus a friendly Pekka needs no further answer.
    Simulation sim{seeded()};
    placeThreat(sim, cards::Goblins);
    sim.board().spawn(sim.cards().get(cards::Pekka), 20, 21, /*isPlayer=*/true, Lane::LEFT);

    StrategicAiController ai{promptButThriftyTuning()};
    const std::size_t before = sim.board().getEntities().size();
    for (int i = 0; i < 20; ++i) {
        ai.update(sim, true, kDefaultTimeStep);
    }

    EXPECT_EQ(sim.board().getEntities().size(), before)
        << "piled more defenders onto a threat already outmatched";
}

TEST(StrategicAi, NeverOverdrawsElixir) {
    Simulation sim{seeded(9)};
    StrategicAiController ai;
    RandomAiController opponent;

    while (sim.isRunning()) {
        ai.update(sim, true, kDefaultTimeStep);
        opponent.update(sim, false, kDefaultTimeStep);
        sim.step();
        ASSERT_GE(sim.elixir(true), 0.0f);
        ASSERT_LE(sim.elixir(true), sim.config().maxElixir);
    }
}

// ---------------------------------------------------------------------------
// It plays a roster it has never seen
// ---------------------------------------------------------------------------

TEST(StrategicAi, ReasonsAboutCustomCardsFromTheirDataAlone) {
    // A roster with no built-in troops at all. The controller has to work out
    // that the threat flies, and that only one of its two options can shoot at
    // it -- from the specs, since it has never heard of any of these cards.
    MatchConfig config = seeded();
    config.cards = CardRegistry();

    CardSpec king = defaultCards().get(cards::KingTower);
    CardSpec queen = defaultCards().get(cards::QueenTower);
    // Strip the towers' anti-air so they cannot cover the threat themselves,
    // forcing the controller to answer it.
    king.targets = TargetFilter{true, false};
    queen.targets = TargetFilter{true, false};
    config.cards.define(king);
    config.cards.define(queen);

    CardSpec flier;
    flier.id = "wyvern";
    flier.symbol = 'Y';
    flier.health = 300;
    flier.damage = 40;
    flier.attackSpeed = 1.0f;
    flier.elixirCost = 3.0f;
    flier.domain = MovementDomain::Air;
    flier.targets = TargetFilter{true, true};
    config.cards.define(flier);

    CardSpec groundOnly;
    groundOnly.id = "pikeman";
    groundOnly.symbol = 'I';
    groundOnly.health = 700;     // tougher and cheaper, so only the filter
    groundOnly.damage = 60;      // should rule it out
    groundOnly.attackSpeed = 1.0f;
    groundOnly.elixirCost = 2.0f;
    groundOnly.targets = TargetFilter{/*ground=*/true, /*air=*/false};
    config.cards.define(groundOnly);

    CardSpec antiAir;
    antiAir.id = "slinger";
    antiAir.symbol = 'S';
    antiAir.health = 200;
    antiAir.damage = 25;
    antiAir.attackSpeed = 1.0f;
    antiAir.attackRange = 6;
    antiAir.elixirCost = 3.0f;
    antiAir.targets = TargetFilter{true, /*air=*/true};
    config.cards.define(antiAir);

    Simulation sim{config};
    sim.board().spawn(sim.cards().get("wyvern"), 20, 20, /*isPlayer=*/false, Lane::LEFT);

    StrategicAiController ai{promptTuning()};
    const std::size_t before = sim.board().getEntities().size();
    ai.update(sim, true, kDefaultTimeStep);

    const Entity* deployed = lastDeployed(sim, before);
    ASSERT_NE(deployed, nullptr) << "did not answer a custom air threat";
    // What matters is that it ruled out the ground-only card, which is the
    // tougher and cheaper of the two and would win on numbers alone. Either
    // air-capable card is a legitimate answer.
    EXPECT_TRUE(deployed->spec().targets.air)
        << "chose " << deployed->cardId() << ", which cannot reach a flier";
    EXPECT_NE(deployed->cardId(), "pikeman");
}

TEST(StrategicAi, CommitsSomethingSubstantialWhenItAttacks) {
    // Nothing to defend against and a full bar: the pick should be a real
    // investment that can travel, not a building and not the cheapest chip
    // unit available.
    //
    // Deliberately not asserting it picks the tower-focused card. That sounds
    // right -- a card that ignores troops reaches the tower -- but weighting the
    // controller to prefer the shipped roster's tower-focused card costs it 14
    // points of win rate, because that card is slow and low-damage.
    MatchConfig config = seeded();
    config.startingElixir = 10.0f;

    Simulation sim{config};
    StrategicAiController ai{promptTuning()};

    const std::size_t before = sim.board().getEntities().size();
    ai.update(sim, true, kDefaultTimeStep);

    const Entity* deployed = lastDeployed(sim, before);
    ASSERT_NE(deployed, nullptr) << "held a full elixir bar with nothing to defend";
    EXPECT_NE(deployed->spec().movement, MovementStyle::Stationary)
        << "attacked with " << deployed->cardId() << ", which cannot advance";
    EXPECT_GE(deployed->spec().elixirCost, 3.0f)
        << "spent only " << deployed->spec().elixirCost << " elixir from a full bar";
    EXPECT_EQ(ai.telemetry().offensiveDeploys, 1u);
}

// ---------------------------------------------------------------------------
// It is actually stronger
// ---------------------------------------------------------------------------

TEST(StrategicAi, BeatsTheRandomControllerConvincingly) {
    // Both sides are measured, because acting first is itself an advantage:
    // between two identical random opponents player one takes about 54%. The
    // strategic controller has to beat that margin from either side.
    const int kMatches = 60;

    auto winRateAsPlayerOne = [](bool strategicIsPlayerOne) {
        int wins = 0;
        for (int seed = 0; seed < kMatches; ++seed) {
            Simulation sim{seeded(static_cast<std::uint64_t>(seed))};
            StrategicAiController strategic;
            RandomAiController random;

            AiController& one = strategicIsPlayerOne ? static_cast<AiController&>(strategic)
                                                     : static_cast<AiController&>(random);
            AiController& two = strategicIsPlayerOne ? static_cast<AiController&>(random)
                                                     : static_cast<AiController&>(strategic);
            while (sim.isRunning()) {
                one.update(sim, true, kDefaultTimeStep);
                two.update(sim, false, kDefaultTimeStep);
                sim.step();
            }

            const bool strategicWon = strategicIsPlayerOne ? (sim.result() == MatchResult::PLAYER_ONE_WINS)
                                                           : (sim.result() == MatchResult::PLAYER_TWO_WINS);
            wins += strategicWon ? 1 : 0;
        }
        return static_cast<double>(wins) / kMatches;
    };

    const double asFirst = winRateAsPlayerOne(true);
    const double asSecond = winRateAsPlayerOne(false);

    EXPECT_GT(asFirst, 0.65) << "only won " << asFirst * 100 << "% while moving first";
    EXPECT_GT(asSecond, 0.65) << "only won " << asSecond * 100 << "% while moving second";
}

TEST(StrategicAi, EndsMatchesWithABetterTowerDifferential) {
    // How it wins, measured: almost entirely by keeping its own towers intact.
    // It deals slightly *less* damage than random play, because it banks elixir
    // and answers pushes instead of trading blindly. So the honest metric is the
    // differential, not damage dealt -- an earlier version of this test asserted
    // the latter and failed for the right reason.
    auto differential = [](bool strategic) {
        int total = 0;
        for (int seed = 0; seed < 40; ++seed) {
            Simulation sim{seeded(static_cast<std::uint64_t>(seed))};
            StrategicAiController smart;
            RandomAiController dumb;
            RandomAiController opponent;
            AiController& ours = strategic ? static_cast<AiController&>(smart) : static_cast<AiController&>(dumb);

            while (sim.isRunning()) {
                ours.update(sim, true, kDefaultTimeStep);
                opponent.update(sim, false, kDefaultTimeStep);
                sim.step();
            }
            total += sim.towerHealth(true) - sim.towerHealth(false);
        }
        return total;
    };

    EXPECT_GT(differential(true), differential(false))
        << "the strategic controller finished no further ahead than random play";
}

TEST(StrategicAi, IsDeterministicForAGivenSeed) {
    auto playOut = [] {
        Simulation sim{seeded(77)};
        StrategicAiController ai;
        RandomAiController opponent;
        while (sim.isRunning()) {
            ai.update(sim, true, kDefaultTimeStep);
            opponent.update(sim, false, kDefaultTimeStep);
            sim.step();
        }
        return std::vector<int>{static_cast<int>(sim.result()), sim.towerHealth(true), sim.towerHealth(false)};
    };

    EXPECT_EQ(playOut(), playOut());
}
