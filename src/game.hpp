#pragma once
#include "board.hpp"
#include "renderer.hpp"
#include <vector>
#include <memory>

class Game {
public:
    Game();
    void run();

private:
    void processInput();
    void update();
    void render();
    void spawnTroop(char type, int x, bool isPlayerOne);
    void handleCombat();
    void updateElixir();
    void switchTurns();

    Board board;
    Renderer renderer;
    bool isRunning;
    float elixirPlayerOne;
    float elixirPlayerTwo;
    float elixirTimer;
    bool isPlayerOneTurn;
    float gameTimer;
    const float GAME_DURATION = 120.0f; // 2 minutes in seconds
    const float ELIXIR_REGEN_RATE = 2.8f;
    const float MAX_ELIXIR = 10.0f;
};
