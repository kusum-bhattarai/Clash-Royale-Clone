// Tests for the card registry -- the library's extension point.
//
// The behavior these cover is the whole reason for the data-driven rewrite:
// defining a unit used to mean editing a header, a source file, the EntityType
// enum, the factory switch and both damage-modifier switches, none of which a
// downstream project can do without forking. Everything here is done from
// outside the library, using only its public interface.

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "clash_royale/ai/random_controller.hpp"
#include "clash_royale/sim/card_registry.hpp"
#include "clash_royale/sim/default_cards.hpp"
#include "clash_royale/sim/simulation.hpp"

#include "test_helpers.hpp"

using namespace cr;
using namespace cr::testing;

namespace {

/// A card a downstream project might add: a fast, fragile flier that savages
/// heavy armor. None of these properties required a library change.
CardSpec harpySpec() {
    CardSpec spec;
    spec.id = "harpy";
    spec.displayName = "Harpy";
    spec.symbol = 'H';
    spec.health = 240;
    spec.damage = 30;
    spec.attackRange = 2;
    spec.moveSpeed = 1.6f;
    spec.attackSpeed = 1.0f / 0.9f;
    spec.elixirCost = 3.0f;
    spec.domain = MovementDomain::Air;
    spec.movement = MovementStyle::Diagonal;
    spec.armor = ArmorClass::Light;
    spec.targets = TargetFilter{/*ground=*/true, /*air=*/true};
    spec.damageModifiers = {
        DamageModifier{2.0f, {ArmorClass::Heavy}, {}, {}, {}},
    };
    return spec;
}

}  // namespace

// ---------------------------------------------------------------------------
// The built-in roster
// ---------------------------------------------------------------------------

TEST(CardRegistry, DefaultRosterHoldsTenCardsEightOfThemDeployable) {
    const CardRegistry& registry = defaultCards();

    EXPECT_EQ(registry.size(), 10u);
    EXPECT_EQ(registry.all().size(), 10u);
    // The two tower types exist on the board but cannot be played.
    EXPECT_EQ(registry.deployable().size(), 8u);

    for (const CardSpec* spec : registry.deployable()) {
        EXPECT_NE(spec->id, std::string(cards::KingTower));
        EXPECT_NE(spec->id, std::string(cards::QueenTower));
    }
}

TEST(CardRegistry, LooksCardsUpAndReportsUnknownOnes) {
    const CardRegistry& registry = defaultCards();

    ASSERT_NE(registry.find(cards::Knight), nullptr);
    EXPECT_EQ(registry.get(cards::Knight).displayName, "Knight");
    EXPECT_TRUE(registry.contains(cards::Knight));

    EXPECT_EQ(registry.find("nonesuch"), nullptr);
    EXPECT_FALSE(registry.contains("nonesuch"));
    EXPECT_THROW(registry.get("nonesuch"), std::out_of_range);
}

TEST(CardRegistry, RejectsAnEmptyId) {
    CardRegistry registry;
    CardSpec nameless;
    EXPECT_THROW(registry.define(nameless), std::invalid_argument);
}

TEST(CardRegistry, SpecReferencesSurviveLaterRegistrations) {
    // Entities hold a `const CardSpec&`, so a reference handed out early must
    // stay valid as more cards arrive. This is why the storage is a deque.
    CardRegistry registry;
    const CardSpec& first = registry.define(harpySpec());
    const CardSpec* address = &first;

    for (int i = 0; i < 256; ++i) {
        CardSpec filler = harpySpec();
        filler.id = "filler" + std::to_string(i);
        registry.define(filler);
    }

    EXPECT_EQ(&registry.get("harpy"), address) << "a spec moved after later cards were defined";
    EXPECT_EQ(first.id, "harpy");
}

TEST(CardRegistry, DefiningAnExistingIdRebalancesItInPlace) {
    CardRegistry registry = CardRegistry::withDefaultCards();
    const CardSpec* before = &registry.get(cards::Knight);

    CardSpec buffed = registry.get(cards::Knight);
    buffed.damage = 999;
    registry.define(buffed);

    EXPECT_EQ(registry.size(), 10u) << "rebalancing should not add a card";
    EXPECT_EQ(registry.get(cards::Knight).damage, 999);
    EXPECT_EQ(&registry.get(cards::Knight), before) << "the address must stay stable";
}

// ---------------------------------------------------------------------------
// Adding a card from outside the library
// ---------------------------------------------------------------------------

TEST(CustomCard, IsDeployableInAMatchWithoutAnyLibraryChange) {
    MatchConfig config;
    config.deterministic = true;
    config.seed = 11;
    config.cards.define(harpySpec());

    Simulation sim{config};
    const size_t before = sim.board().getEntities().size();

    ASSERT_TRUE(sim.deploy("harpy", Lane::RIGHT, /*isPlayerOne=*/true));

    EXPECT_EQ(sim.board().getEntities().size(), before + 1);
    EXPECT_FLOAT_EQ(sim.elixir(true), 5.0f - 3.0f);

    const auto& harpy = sim.board().getEntities().back();
    EXPECT_EQ(harpy->cardId(), "harpy");
    EXPECT_EQ(harpy->getMaxHealth(), 240);
    EXPECT_TRUE(harpy->isFlying());
    EXPECT_TRUE(harpy->canAttackAir());
    EXPECT_EQ(harpy->getSymbol(), 'H');
}

TEST(CustomCard, UsesTheLowercaseGlyphForTheOpponent) {
    CardRegistry registry;
    const CardSpec& spec = registry.define(harpySpec());

    EXPECT_EQ(Entity(spec, 10, 10, /*isPlayer=*/true, Lane::LEFT).getSymbol(), 'H');
    EXPECT_EQ(Entity(spec, 10, 10, /*isPlayer=*/false, Lane::LEFT).getSymbol(), 'h');
}

TEST(CustomCard, DamageModifiersApplyInCombat) {
    CardRegistry registry = CardRegistry::withDefaultCards();
    const CardSpec& harpy = registry.define(harpySpec());

    Board board;
    board.setCombatRules(CombatRules{/*criticalChance=*/0.0f, 1.5f});
    board.addEntity(createEntity(harpy, 10, 10, true, Lane::LEFT));
    // PEKKA is heavy armor, which the Harpy doubles damage against.
    board.addEntity(createEntity(registry.get(cards::Pekka), 10, 11, false, Lane::LEFT));

    Rng rng{1};
    const int initial = board.getEntities().back()->getHealth();
    board.handleCombat(rng, 0.1f);

    // 30 damage, doubled against heavy armor, then scaled by PEKKA's 0.6
    // incoming multiplier: floor(30 * 2.0 * 0.6) = 36.
    EXPECT_EQ(initial - board.getEntities().back()->getHealth(), 36);
}

TEST(CustomCard, TargetFilterGovernsWhatItWillAttack) {
    CardRegistry registry = CardRegistry::withDefaultCards();

    CardSpec groundOnly = harpySpec();
    groundOnly.id = "ground_only";
    groundOnly.targets = TargetFilter{/*ground=*/true, /*air=*/false};
    const CardSpec& spec = registry.define(groundOnly);

    Board board;
    auto unit = createEntity(spec, 10, 10, true, Lane::LEFT);
    board.addEntity(unit);
    board.addEntity(createEntity(registry.get(cards::Dragon), 10, 14, false, Lane::LEFT));

    EXPECT_EQ(unit->findTarget(board), nullptr) << "a ground-only filter admitted a flier";
}

TEST(CustomCard, BuildingsOnlyFilterWalksPastTroops) {
    CardRegistry registry = CardRegistry::withDefaultCards();

    CardSpec siege = harpySpec();
    siege.id = "siege";
    siege.domain = MovementDomain::Ground;
    siege.targets = TargetFilter{true, false, /*buildingsOnly=*/true};
    const CardSpec& spec = registry.define(siege);

    Board board;
    auto unit = createEntity(spec, 10, 10, true, Lane::LEFT);
    board.addEntity(unit);
    board.addEntity(createEntity(registry.get(cards::Knight), 10, 11, false, Lane::LEFT));
    board.addEntity(createEntity(registry.get(cards::QueenTower), 10, 25, false, Lane::LEFT));

    const auto target = unit->findTarget(board);
    ASSERT_NE(target, nullptr);
    EXPECT_EQ(target->cardId(), std::string(cards::QueenTower))
        << "buildingsOnly did not skip the closer troop";
}

TEST(CustomCard, IsPickedUpByTheDefaultAiAutomatically) {
    // The AI reads its roster from the registry rather than a hardcoded list.
    // Retiring the built-in troops leaves the Harpy as the only legal pick, so
    // any deploy at all proves the roster is data-driven.
    MatchConfig config;
    config.deterministic = true;
    config.seed = 3;
    config.cards.define(harpySpec());
    // Copy the list first: define() rebuilds the vector that deployable()
    // returns, so iterating it while registering would invalidate it.
    const std::vector<const CardSpec*> roster = config.cards.deployable();
    for (const CardSpec* spec : roster) {
        if (spec->id != "harpy") {
            CardSpec retired = *spec;
            retired.deployable = false;
            config.cards.define(retired);
        }
    }
    ASSERT_EQ(config.cards.deployable().size(), 1u);

    Simulation sim{config};
    const size_t towers = sim.board().getEntities().size();

    RandomAiController ai{/*deployChancePerStep=*/1.0f, /*minimumElixir=*/0.0f};
    ai.update(sim, /*isPlayerOne=*/false, kDefaultTimeStep);

    ASSERT_EQ(sim.board().getEntities().size(), towers + 1) << "AI deployed nothing";
    EXPECT_EQ(sim.board().getEntities().back()->cardId(), "harpy");
}

TEST(CustomCard, ARosterWithoutTowerCardsFailsWithAnActionableMessage) {
    MatchConfig config;
    config.cards = CardRegistry{};  // no cards at all
    config.cards.define(harpySpec());

    try {
        Simulation sim{config};
        FAIL() << "expected construction to be rejected";
    } catch (const std::invalid_argument& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("king_tower"), std::string::npos);
        EXPECT_NE(message.find("kingTowerCard"), std::string::npos)
            << "the message should name the setting that fixes it";
    }
}

TEST(CustomCard, TowerCardsCanBeRedirectedToACustomRoster) {
    CardSpec keep = harpySpec();
    keep.id = "fortress";
    keep.isBuilding = true;
    keep.movement = MovementStyle::Stationary;
    keep.moveSpeed = 0.0f;
    keep.health = 3000;
    keep.towerRole = TowerRole::King;
    keep.deployable = false;

    CardSpec outpost = keep;
    outpost.id = "outpost";
    outpost.health = 900;
    outpost.towerRole = TowerRole::Queen;

    MatchConfig config;
    config.cards = CardRegistry{};
    config.cards.define(keep);
    config.cards.define(outpost);
    config.cards.define(harpySpec());
    config.kingTowerCard = "fortress";
    config.queenTowerCard = "outpost";

    Simulation sim{config};

    EXPECT_EQ(sim.board().getEntities().size(), 6u);
    EXPECT_EQ(sim.towerHealth(true), 3000 + 900 + 900);
    EXPECT_EQ(sim.board().getEntities().front()->cardId(), "fortress");
}

TEST(CustomCard, UnknownIdsAreRefusedRatherThanThrowing) {
    Simulation sim;
    EXPECT_FALSE(sim.canAfford("not_a_card", true));
    EXPECT_FALSE(sim.deploy("not_a_card", Lane::LEFT, true));
    EXPECT_EQ(sim.board().getEntities().size(), 6u);
}

TEST(CustomCard, NonDeployableCardsCannotBePlayed) {
    Simulation sim;
    // Towers are on the board but are not cards a player may deploy.
    EXPECT_FALSE(sim.canAfford(cards::KingTower, true));
    EXPECT_FALSE(sim.deploy(cards::KingTower, Lane::LEFT, true));
}

// ---------------------------------------------------------------------------
// The behavior escape hatch
// ---------------------------------------------------------------------------

namespace {

/// A unit whose behavior cannot be expressed as data: it refuses to move at
/// all once it is below half health.
class CautiousEntity : public Entity {
public:
    using Entity::Entity;

protected:
    void move(const Board& board) override {
        if (getHealth() * 2 < getMaxHealth()) {
            return;
        }
        Entity::move(board);
    }
};

}  // namespace

TEST(CustomCard, FactoryHookSuppliesACustomEntitySubclass) {
    CardRegistry registry = CardRegistry::withDefaultCards();

    CardSpec cautious = harpySpec();
    cautious.id = "cautious";
    cautious.domain = MovementDomain::Ground;
    cautious.movement = MovementStyle::AxisStep;
    cautious.factory = [](const CardSpec& spec, int x, int y, bool isPlayer, Lane lane) {
        return std::make_shared<CautiousEntity>(spec, x, y, isPlayer, lane);
    };
    const CardSpec& spec = registry.define(cautious);

    Board board;
    auto unit = createEntity(spec, 10, 10, true, Lane::LEFT);
    board.addEntity(unit);
    board.addEntity(createEntity(registry.get(cards::QueenTower), 10, 25, false, Lane::LEFT));

    // Healthy: it advances.
    for (int i = 0; i < 20; ++i) {
        unit->update(board, 0.1f);
    }
    EXPECT_GT(unit->getY(), 10) << "the custom subclass never moved while healthy";

    // Wounded past half: it holds position.
    unit->takeDamage(unit->getMaxHealth() - 1);
    const int holding = unit->getY();
    for (int i = 0; i < 40; ++i) {
        unit->update(board, 0.1f);
    }
    EXPECT_EQ(unit->getY(), holding) << "the custom move() override was not honoured";
}
