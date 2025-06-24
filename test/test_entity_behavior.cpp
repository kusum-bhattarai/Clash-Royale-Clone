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