#include <gtest/gtest.h>
#include "core/EntityFactory.hpp"
#include "entity/Knight.hpp" 
#include "entity/entity.hpp"

TEST(EntityFactoryTest, CreatesKnightCorrectly) {
    EntityType type = EntityType::KNIGHT;
    int x = 10;
    int y = 15;
    bool isPlayer = true;
    Lane lane = Lane::LEFT;

    // calling the create function to test
    auto knight = EntityFactory::create(type, x, y, isPlayer, lane);

    // Assert: Check if the result is what we expect.
    ASSERT_NE(knight, nullptr); 
    
    // Now check its properties.
    EXPECT_EQ(knight->getType(), EntityType::KNIGHT);
    EXPECT_EQ(knight->getHealth(), 600);
    EXPECT_EQ(knight->getX(), 10);
    EXPECT_EQ(knight->getY(), 15);
    EXPECT_EQ(knight->getIsPlayer(), true);
    EXPECT_EQ(knight->getLane(), Lane::LEFT);
}

TEST(EntityFactoryTest, CreatesKingTowerCorrectly) {
    // King Tower creation
    auto kingTower = EntityFactory::create(EntityType::KING_TOWER, 20, 3, false, Lane::RIGHT);

    // Assert the expected properties
    ASSERT_NE(kingTower, nullptr); // Ensure it was created
    EXPECT_EQ(kingTower->getType(), EntityType::KING_TOWER);
    EXPECT_EQ(kingTower->getHealth(), 4000);
    EXPECT_EQ(kingTower->getDamage(), 75);
    EXPECT_EQ(kingTower->getAttackRange(), 7);
    EXPECT_TRUE(kingTower->canAttackAir());
}

TEST(EntityFactoryTest, CreatesQueenTowerCorrectly) {
    // Queen Tower creation
    auto queenTower = EntityFactory::create(EntityType::QUEEN_TOWER, 5, 25, true, Lane::LEFT);

    // Assert the expected properties
    ASSERT_NE(queenTower, nullptr);
    EXPECT_EQ(queenTower->getType(), EntityType::QUEEN_TOWER);
    EXPECT_EQ(queenTower->getHealth(), 1500);
    EXPECT_EQ(queenTower->getDamage(), 50);
    EXPECT_EQ(queenTower->getAttackRange(), 5);
}

TEST(EntityFactoryTest, CreatesDragonCorrectly) {
    // Dragon creation
    auto dragon = EntityFactory::create(EntityType::DRAGON, 10, 10, true, Lane::LEFT);

    // Assert the expected properties
    ASSERT_NE(dragon, nullptr);
    EXPECT_EQ(dragon->getType(), EntityType::DRAGON);
    EXPECT_TRUE(dragon->isFlying());
    EXPECT_TRUE(dragon->canAttackAir());
    EXPECT_EQ(dragon->getDamage(), 50);
}

TEST(EntityFactoryTest, CreatesCanonCorrectly) {
    // Canon creation
    auto canon = EntityFactory::create(EntityType::CANON, 15, 20, true, Lane::RIGHT);

    // Assert the expected properties
    ASSERT_NE(canon, nullptr);
    EXPECT_EQ(canon->getType(), EntityType::CANON);
    EXPECT_FALSE(canon->isFlying());
    EXPECT_FALSE(canon->canAttackAir()); // A key property of the Canon
    EXPECT_EQ(canon->getHealth(), 500);
}