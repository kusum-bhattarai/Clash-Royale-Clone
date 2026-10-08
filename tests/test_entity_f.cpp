#include <gtest/gtest.h>

#include <string_view>
#include "test_helpers.hpp"
#include "clash_royale/sim/entity.hpp"

using namespace cr;
using namespace cr::testing;

TEST(CardCreation, CreatesKnightCorrectly) {
    const std::string_view type = cards::Knight;
    int x = 10;
    int y = 15;
    bool isPlayer = true;
    Lane lane = Lane::LEFT;

    // calling the create function to test
    auto knight = spawn(type, x, y, isPlayer, lane);

    // Assert: Check if the result is what we expect.
    ASSERT_NE(knight, nullptr); 
    
    // Now check its properties.
    EXPECT_EQ(knight->cardId(), cards::Knight);
    EXPECT_EQ(knight->getHealth(), 600);
    EXPECT_EQ(knight->getX(), 10);
    EXPECT_EQ(knight->getY(), 15);
    EXPECT_EQ(knight->getIsPlayer(), true);
    EXPECT_EQ(knight->getLane(), Lane::LEFT);
}

TEST(CardCreation, CreatesKingTowerCorrectly) {
    // King Tower creation
    auto kingTower = spawn(cards::KingTower, 20, 3, false, Lane::RIGHT);

    // Assert the expected properties
    ASSERT_NE(kingTower, nullptr); // Ensure it was created
    EXPECT_EQ(kingTower->cardId(), cards::KingTower);
    EXPECT_EQ(kingTower->getHealth(), 4000);
    EXPECT_EQ(kingTower->getDamage(), 75);
    EXPECT_EQ(kingTower->getAttackRange(), 7);
    EXPECT_TRUE(kingTower->canAttackAir());
}

TEST(CardCreation, CreatesQueenTowerCorrectly) {
    // Queen Tower creation
    auto queenTower = spawn(cards::QueenTower, 5, 25, true);

    // Assert the expected properties
    ASSERT_NE(queenTower, nullptr);
    EXPECT_EQ(queenTower->cardId(), cards::QueenTower);
    EXPECT_EQ(queenTower->getHealth(), 1500);
    EXPECT_EQ(queenTower->getDamage(), 50);
    EXPECT_EQ(queenTower->getAttackRange(), 5);
}

TEST(CardCreation, CreatesDragonCorrectly) {
    // Dragon creation
    auto dragon = spawn(cards::Dragon, 10, 10, true);

    // Assert the expected properties
    ASSERT_NE(dragon, nullptr);
    EXPECT_EQ(dragon->cardId(), cards::Dragon);
    EXPECT_TRUE(dragon->isFlying());
    EXPECT_TRUE(dragon->canAttackAir());
    EXPECT_EQ(dragon->getDamage(), 50);
}

TEST(CardCreation, CreatesCanonCorrectly) {
    // Canon creation
    auto canon = spawn(cards::Canon, 15, 20, true, Lane::RIGHT);

    // Assert the expected properties
    ASSERT_NE(canon, nullptr);
    EXPECT_EQ(canon->cardId(), cards::Canon);
    EXPECT_FALSE(canon->isFlying());
    EXPECT_FALSE(canon->canAttackAir()); // A key property of the Canon
    EXPECT_EQ(canon->getHealth(), 500);
}