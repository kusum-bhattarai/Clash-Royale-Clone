#pragma once
#include "core/board.hpp"
#include "core/renderer.hpp"
#include "core/input_handler.hpp"
#include <vector>
#include <memory>

enum class Lane {
    LEFT,
    RIGHT,
    CENTER
};

enum class GameState {
    SELECTING_TROOP,
    SELECTING_LANE
};

class Game {
public:
    Game();
    virtual ~Game() = default;
    void run();

    // --- Public Getters for assertions ---
    bool getIsRunning() const { return isRunning; }
    float getElixirPlayerOne() const { return elixirPlayerOne; }
    float getElixirPlayerTwo() const { return elixirPlayerTwo; }
    Board& getBoard() { return board; }

protected:
    //for testing
    virtual void updateElixir();
    virtual void runAI();
    virtual void handleCombat();
    virtual void updateEntities();
    virtual void checkWinCondition();

private:
    void processInput();
    void update();
    void render();
    void spawnTroop(EntityType type, Lane lane, bool isPlayerOne);

    Board board;
    Renderer renderer;
    InputHandler inputHandler;
    bool isRunning;
    float elixirPlayerOne;
    float elixirPlayerTwo;
    float elixirTimer;
    float gameTimer;
    int renderCounter; 
    const float GAME_DURATION = 120.0f;
    const float ELIXIR_REGEN_RATE = 2.8f;
    const float MAX_ELIXIR = 10.0f;
    GameState currentGameState;
    EntityType pendingTroopType;
};