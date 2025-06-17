#pragma once
#include "board.hpp"
#include "renderer.hpp"
#include "input_handler.hpp"
#include <vector>
#include <memory>

class Game {
public:
    Game();
    void run();

private:
    void processInput();
    void runAI();
    void update();
    void render();
    void spawnTroop(char type, int x, bool isPlayerOne);
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
};