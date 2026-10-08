#include <gtest/gtest.h>
#include "clash_royale/core/rng.hpp"
#include "test_helpers.hpp"
#include "clash_royale/sim/board.hpp"
#include "clash_royale/sim/entity.hpp"

using namespace cr;
using namespace cr::testing;

// Test Fixture for Damage Tests
class DamageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // This code runs before each test in this suite.
        knight = spawn(cards::Knight, 10, 10, true);
    }

    std::shared_ptr<Entity> knight;
};

TEST_F(DamageTest, EntityTakesDamageCorrectly) {
    int initialHealth = knight->getHealth();
    knight->takeDamage(100);
    
    EXPECT_EQ(knight->getHealth(), initialHealth - 100);
}

TEST_F(DamageTest, EntityDiesWhenHealthReachesZero) {
    knight->takeDamage(knight->getHealth()); // Deal exact lethal damage

    EXPECT_EQ(knight->getHealth(), 0);
    EXPECT_FALSE(knight->isAlive());
}

TEST_F(DamageTest, EntityHealthDoesNotGoBelowZero) {
    knight->takeDamage(knight->getHealth() + 500); // Deal overkill damage

    EXPECT_EQ(knight->getHealth(), 0);
    EXPECT_FALSE(knight->isAlive());
}

// Test Suite for Movement
TEST(MovementTest, StationaryEntitiesDoNotMove) {
    Board board;
    auto kingTower = spawn(cards::KingTower, 10, 3, false);
    int startX = kingTower->getX();
    int startY = kingTower->getY();

    // Updating the entity multiple times to see if tower moves
    for (int i = 0; i < 20; ++i) {
        kingTower->update(board);
    }

    // Assert: Position should be unchanged.
    EXPECT_EQ(kingTower->getX(), startX);
    EXPECT_EQ(kingTower->getY(), startY);
}

TEST(MovementTest, KnightMovesTowardTarget) {
    Board board;
    auto knight = spawn(cards::Knight, 10, 10, true);
    auto enemy = spawn(cards::Knight, 10, 20, false);
    board.addEntity(knight);
    board.addEntity(enemy);

    // movement updates
    for (int i = 0; i < 10; ++i) {
        knight->update(board);
    }

    // Assert: Knight should have moved one step closer to the enemy on the Y-axis.
    EXPECT_EQ(knight->getX(), 10);
    EXPECT_EQ(knight->getY(), 11);
}

// Test Suite for the main combat logic on the board
TEST(CombatTest, KnightAttacksEnemyInRange) {
    Board board;
    auto playerKnight = spawn(cards::Knight, 10, 10, true);
    auto enemyKnight = spawn(cards::Knight, 10, 11, false);
    
    int enemyInitialHealth = enemyKnight->getHealth();

    board.addEntity(playerKnight);
    board.addEntity(enemyKnight);

    // Act: Running the combat logic
    Rng rng{1};
    board.handleCombat(rng);

    // Assert: Check if the enemy knight has taken damage.
    // The exact damage depends on calculateDamage logic, but it should be less than its initial health.
    EXPECT_LT(enemyKnight->getHealth(), enemyInitialHealth);
    EXPECT_GT(enemyKnight->getHealth(), 0); // To ensure it wasn't a one-hit kill
}

TEST(CombatTest, KnightDoesNotAttackOutOfRange) {
    Board board;
    auto playerKnight = spawn(cards::Knight, 10, 10, true);
    auto enemyKnight = spawn(cards::Knight, 10, 20, false);

    int enemyInitialHealth = enemyKnight->getHealth();

    board.addEntity(playerKnight);
    board.addEntity(enemyKnight);

    // Act: combat logic in action
    Rng rng{1};
    board.handleCombat(rng);

    // Assert: The enemy's health should be unchanged.
    EXPECT_EQ(enemyKnight->getHealth(), enemyInitialHealth);
}

TEST(CombatTest, GolemPrioritizesTowerOverCloserTroop) {
    // Placing golem in range of both
    Board board;
    auto golem = spawn(cards::Golem, 10, 10, true);
    auto enemyTower = spawn(cards::QueenTower, 10, 25, false);
    auto decoyKnight = spawn(cards::Knight, 10, 11, false);
    
    board.addEntity(golem);
    board.addEntity(enemyTower);
    board.addEntity(decoyKnight);

    // ACT: Calling the Golem's findTarget method directly to test its "brain".
    auto chosenTarget = golem->findTarget(board);

    // ASSERT: The chosen target should not be null and should be the tower.
    ASSERT_NE(chosenTarget, nullptr);
    EXPECT_EQ(chosenTarget->cardId(), cards::QueenTower);
}

TEST(CombatTest, CanonCannotAttackFlyingDragon) {
    // Placing a dragon in range of a canon.
    Board board;
    auto canon = spawn(cards::Canon, 15, 20, true, Lane::RIGHT);
    auto dragon = spawn(cards::Dragon, 15, 22, false, Lane::RIGHT); // Well within range

    int dragonInitialHealth = dragon->getHealth();

    board.addEntity(canon);
    board.addEntity(dragon);

    Rng rng{1};
    board.handleCombat(rng);

    // ASSERT: Dragon's health should be unchanged because the canon cannot attack air.
    EXPECT_EQ(dragon->getHealth(), dragonInitialHealth);
}

TEST(CombatTest, ArcherCanAttackFlyingDragon) {
    // Placing a dragon in range of an archer.
    Board board;
    auto archer = spawn(cards::Archers, 15, 20, true, Lane::RIGHT);
    auto dragon = spawn(cards::Dragon, 15, 25, false, Lane::RIGHT); // Within Archer's range of 7

    int dragonInitialHealth = dragon->getHealth();

    board.addEntity(archer);
    board.addEntity(dragon);

    Rng rng{1};
    board.handleCombat(rng);

    // ASSERT: Dragon's health should decrease because the Archer can attack air.
    EXPECT_LT(dragon->getHealth(), dragonInitialHealth);
}

// Pekka's heavy armor
TEST(AdvancedCombatTest, PekkaHasDamageResistance) {
    Board board;
    auto pekka = spawn(cards::Pekka, 10, 10, true, Lane::RIGHT);
    auto enemyArchers = spawn(cards::Archers, 10, 15, false, Lane::RIGHT);

    int pekkaInitialHealth = pekka->getHealth();
    int archerBaseDamage = enemyArchers->getDamage(); // Should be 20

    board.addEntity(pekka);
    board.addEntity(enemyArchers);

    Rng rng{1};
    board.handleCombat(rng);
    // ASSERT
    // PEKKA has heavy armor, taking 0.6x damage.
    // Expected damage taken = floor(20 * 0.6) = 12.
    int damageTaken = pekkaInitialHealth - pekka->getHealth();
    EXPECT_LT(damageTaken, archerBaseDamage);
    EXPECT_GT(damageTaken, 0); 
}
