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
