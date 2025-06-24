#include <gtest/gtest.h>
#include "core/EntityFactory.hpp"
#include "core/board.hpp"
#include "entity/entity.hpp"

// Test Fixture for Damage Tests
class DamageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // This code runs before each test in this suite.
        knight = EntityFactory::create(EntityType::KNIGHT, 10, 10, true, Lane::LEFT);
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
    auto kingTower = EntityFactory::create(EntityType::KING_TOWER, 10, 3, false, Lane::LEFT);
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
    auto knight = EntityFactory::create(EntityType::KNIGHT, 10, 10, true, Lane::LEFT);
    auto enemy = EntityFactory::create(EntityType::KNIGHT, 10, 20, false, Lane::LEFT);
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
    auto playerKnight = EntityFactory::create(EntityType::KNIGHT, 10, 10, true, Lane::LEFT);
    auto enemyKnight = EntityFactory::create(EntityType::KNIGHT, 10, 11, false, Lane::LEFT);
    
    int enemyInitialHealth = enemyKnight->getHealth();

    board.addEntity(playerKnight);
    board.addEntity(enemyKnight);

    // Act: Running the combat logic
    board.handleCombat();

    // Assert: Check if the enemy knight has taken damage.
    // The exact damage depends on calculateDamage logic, but it should be less than its initial health.
    EXPECT_LT(enemyKnight->getHealth(), enemyInitialHealth);
    EXPECT_GT(enemyKnight->getHealth(), 0); // To ensure it wasn't a one-hit kill
}

TEST(CombatTest, KnightDoesNotAttackOutOfRange) {
    Board board;
    auto playerKnight = EntityFactory::create(EntityType::KNIGHT, 10, 10, true, Lane::LEFT);
    auto enemyKnight = EntityFactory::create(EntityType::KNIGHT, 10, 20, false, Lane::LEFT);

    int enemyInitialHealth = enemyKnight->getHealth();

    board.addEntity(playerKnight);
    board.addEntity(enemyKnight);

    // Act: combat logic in action
    board.handleCombat();

    // Assert: The enemy's health should be unchanged.
    EXPECT_EQ(enemyKnight->getHealth(), enemyInitialHealth);
}

TEST(CombatTest, GolemPrioritizesTowerOverCloserTroop) {
    // Placing golem in range of both
    Board board;
    auto golem = EntityFactory::create(EntityType::GOLEM, 10, 10, true, Lane::LEFT);
    auto enemyTower = EntityFactory::create(EntityType::QUEEN_TOWER, 10, 25, false, Lane::LEFT);
    auto decoyKnight = EntityFactory::create(EntityType::KNIGHT, 10, 11, false, Lane::LEFT);
    
    board.addEntity(golem);
    board.addEntity(enemyTower);
    board.addEntity(decoyKnight);

    // ACT: Calling the Golem's findTarget method directly to test its "brain".
    auto chosenTarget = golem->findTarget(board);

    // ASSERT: The chosen target should not be null and should be the tower.
    ASSERT_NE(chosenTarget, nullptr);
    EXPECT_EQ(chosenTarget->getType(), EntityType::QUEEN_TOWER);
}

TEST(CombatTest, CanonCannotAttackFlyingDragon) {
    // Placing a dragon in range of a canon.
    Board board;
    auto canon = EntityFactory::create(EntityType::CANON, 15, 20, true, Lane::RIGHT);
    auto dragon = EntityFactory::create(EntityType::DRAGON, 15, 22, false, Lane::RIGHT); // Well within range

    int dragonInitialHealth = dragon->getHealth();

    board.addEntity(canon);
    board.addEntity(dragon);

    board.handleCombat();

    // ASSERT: Dragon's health should be unchanged because the canon cannot attack air.
    EXPECT_EQ(dragon->getHealth(), dragonInitialHealth);
}

TEST(CombatTest, ArcherCanAttackFlyingDragon) {
    // Placing a dragon in range of an archer.
    Board board;
    auto archer = EntityFactory::create(EntityType::ARCHERS, 15, 20, true, Lane::RIGHT);
    auto dragon = EntityFactory::create(EntityType::DRAGON, 15, 25, false, Lane::RIGHT); // Within Archer's range of 7

    int dragonInitialHealth = dragon->getHealth();

    board.addEntity(archer);
    board.addEntity(dragon);

    board.handleCombat();

    // ASSERT: Dragon's health should decrease because the Archer can attack air.
    EXPECT_LT(dragon->getHealth(), dragonInitialHealth);
}

// Pekka's heavy armor
TEST(AdvancedCombatTest, PekkaHasDamageResistance) {
    Board board;
    auto pekka = EntityFactory::create(EntityType::PEKKA, 10, 10, true, Lane::RIGHT);
    auto enemyArchers = EntityFactory::create(EntityType::ARCHERS, 10, 15, false, Lane::RIGHT);

    int pekkaInitialHealth = pekka->getHealth();
    int archerBaseDamage = enemyArchers->getDamage(); // Should be 20

    board.addEntity(pekka);
    board.addEntity(enemyArchers);

    board.handleCombat();
    // ASSERT
    // PEKKA has heavy armor, taking 0.6x damage.
    // Expected damage taken = floor(20 * 0.6) = 12.
    int damageTaken = pekkaInitialHealth - pekka->getHealth();
    EXPECT_LT(damageTaken, archerBaseDamage);
    EXPECT_GT(damageTaken, 0); 
}
