#include <gtest/gtest.h>
#include "core/game.hpp"
#include "core/EntityFactory.hpp"
#include "core/board.hpp"

// This is the "Testing Subclass". 
//It inherits from Game and its sole purpose is to make the protected methods from Game callable.
class TestableGame : public Game {
public:
    void test_updateElixir() { this->updateElixir(); }
    void test_runAI() { this->runAI(); }
    void test_handleCombat() { this->handleCombat(); }
    void test_updateEntities() { this->updateEntities(); }
    void test_checkWinCondition() { this->checkWinCondition(); }
};

class GameLogicTest : public ::testing::Test {
protected:
    TestableGame game;
};

TEST_F(GameLogicTest, ElixirGeneratesAndCapsCorrectly) {
    float initialElixir = game.getElixirPlayerOne();
    // 1 elixir should be generated every 2.8 seconds (28 ticks).

    // Generating elixir for 3 seconds (30 ticks).
    for (int i = 0; i < 30; ++i) {
        game.test_updateElixir();
    }

    // ASSERT I: After 3 seconds, exactly one tick of elixir should have been generated.
    ASSERT_FLOAT_EQ(game.getElixirPlayerOne(), initialElixir + 1.0f);

    // Generating a large amount of elixir to test the cap.
    for (int i = 0; i < 300; ++i) { // 30 seconds worth of ticks
        game.test_updateElixir();
    }

    // ASSERT II: To check that elixir does not exceed the maximum value of 10.
    EXPECT_LE(game.getElixirPlayerOne(), 10.0f);
}

TEST_F(GameLogicTest, AIDeploysTroopWithSufficientElixir) {

    std::srand(0);

    Board& board = game.getBoard();
    size_t initialEntityCount = board.getEntities().size();

    // The AI has a 30% chance to deploy if elixir is >= 3.0.
    for (int i = 0; i < 200; ++i) { // Generate lots of elixir
        game.test_updateElixir();
    }
    ASSERT_GE(game.getElixirPlayerTwo(), 3.0f);

    bool troopWasDeployed = false;
    for (int i = 0; i < 20; ++i) {
        game.test_runAI();
        if (board.getEntities().size() > initialEntityCount) {
            troopWasDeployed = true;
            break; 
        }
    }

    EXPECT_TRUE(troopWasDeployed);
}

TEST_F(GameLogicTest, GameEndsWhenKingTowerIsDestroyed) {
    Board& board = game.getBoard();
    board.getEntities().clear(); // Start with an empty board.

    // Add an enemy King Tower with only 10 health left.
    auto enemyKing = EntityFactory::create(EntityType::KING_TOWER, 10, 20, false, Lane::CENTER);
    enemyKing->takeDamage(3990);
    board.addEntity(enemyKing);

    // Add a powerful PEKKA right next to it to guarantee a quick kill.
    auto playerPekka = EntityFactory::create(EntityType::PEKKA, 10, 19, true, Lane::CENTER);
    board.addEntity(playerPekka);

    // Check that the game is running at the start.
    ASSERT_TRUE(game.getIsRunning());

    // Calling the individual logic steps in the correct order.
    game.test_handleCombat();    // The PEKKA attacks and destroys the tower.
    game.test_updateEntities();  // The board removes the dead tower.
    game.test_checkWinCondition(); // The game logic checks if a king is missing.

    // ASSERT: The game should have correctly identified the win and stopped running.
    EXPECT_FALSE(game.getIsRunning());
}