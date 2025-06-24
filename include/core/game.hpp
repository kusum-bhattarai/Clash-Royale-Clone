#pragma once
#include "core/board.hpp"
#include "core/renderer.hpp"
#include "core/input_handler.hpp"
#include <vector>
#include <memory>

enum class Lane {
    LEFT,
    RIGHT
};

enum class GameState {
    SELECTING_TROOP,
    SELECTING_LANE
};

class Game {
public:
    Game();
    void run();

private:
    void processInput();
    void runAI();
    void update();
    void render();
    void spawnTroop(EntityType type, Lane lane, bool isPlayerOne);
    void handleCombat();
    void updateElixir();

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