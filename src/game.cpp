#include "game.hpp"
#include <thread>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>

Game::Game() : isRunning(true), elixirPlayerOne(5.0f), elixirPlayerTwo(5.0f), 
               elixirTimer(0.0f), gameTimer(0.0f), renderCounter(0) {
    std::srand(std::time(nullptr));
    
    // Initialize Player 2 (AI) towers at the top
    board.addEntity(std::make_shared<Entity>(EntityType::KING_TOWER, 13, 2, false, 4000));
    board.addEntity(std::make_shared<Entity>(EntityType::QUEEN_TOWER, 6, 2, false, 1500));
    board.addEntity(std::make_shared<Entity>(EntityType::QUEEN_TOWER, 20, 2, false, 1500));
    
    // Initialize Player 1 (User) towers at the bottom
    board.addEntity(std::make_shared<Entity>(EntityType::KING_TOWER, 13, 27, true, 4000));
    board.addEntity(std::make_shared<Entity>(EntityType::QUEEN_TOWER, 6, 27, true, 1500));
    board.addEntity(std::make_shared<Entity>(EntityType::QUEEN_TOWER, 20, 27, true, 1500));
}

void Game::run() {
    while (isRunning) {
        processInput(); // Handle user input
        update();       // Update game state, including AI moves
        render();       // Draw the result
        std::this_thread::sleep_for(std::chrono::milliseconds(200)); // Game speed
    }
}

void Game::processInput() {
    auto input = inputHandler.getInput();
    if (!input.has_value()) return;

    switch (*input) {
        case 'q':
            isRunning = false;
            return;
        case 'k': // Knight (4)
        case 'g': // Golem (5)
        case 'p': // Pekka (4)
        case 'b': // Goblins (3)
        case 'd': // Dragon (5)
        case 'w': // Wizard (4)
        case 'a': // Archers (2)
        case 'c': // Canon (3)
            {
                float cost = (*input == 'g' || *input == 'd') ? 5.0f :
                            (*input == 'a') ? 2.0f :
                            (*input == 'b' || *input == 'c') ? 3.0f : 4.0f;
                    
                if (elixirPlayerOne >= cost) {
                    // Player 1 (user) is always 'true'
                    spawnTroop(std::toupper(*input), 0, true);
                    elixirPlayerOne -= cost;
                }
            }
            break;
    }
}

void Game::runAI() {
    // A simple AI that has a chance to deploy a troop if it has enough elixir
    if (elixirPlayerTwo >= 3.0f) { // AI needs at least 3 elixir to consider a move
        bool shouldDeploy = (rand() % 10) < 3; // 30% chance to deploy each update cycle
        if (shouldDeploy) {
            char troopTypes[] = {'k', 'g', 'p', 'b', 'd', 'w', 'a', 'c'};
            char type = troopTypes[rand() % sizeof(troopTypes)];

            float cost = (type == 'g' || type == 'd') ? 5.0f :
                         (type == 'a') ? 2.0f :
                         (type == 'b' || type == 'c') ? 3.0f : 4.0f;
            
            if (elixirPlayerTwo >= cost) {
                // Player 2 (AI) is always 'false'
                spawnTroop(std::toupper(type), 0, false);
                elixirPlayerTwo -= cost;
            }
        }
    }
}

void Game::update() {
    gameTimer += 0.2f; // Each loop is 200ms
    if (gameTimer >= GAME_DURATION) {
        int p1Health = 0, p2Health = 0;
        for (const auto& entity : board.getEntities()) {
            if (entity->getType() == EntityType::KING_TOWER || 
                entity->getType() == EntityType::QUEEN_TOWER) {
                if (entity->getIsPlayer()) { // Player 1's towers
                    p1Health += entity->getHealth();
                } else { // Player 2's towers
                    p2Health += entity->getHealth();
                }
            }
        }
        
        // Determine winner based on remaining tower health
        std::cout << "\nGame Over!\n";
        if (p1Health > p2Health) {
            std::cout << "Winner: Player 1 (You)!\n";
        } else if (p2Health > p1Health) {
            std::cout << "Winner: Player 2 (AI)!\n";
        } else {
            std::cout << "It's a draw!\n";
        }
        isRunning = false;
        return;
    }
    
    runAI(); // Let the AI make a move
    updateElixir();
    board.updateEntities();
    handleCombat();
}

void Game::updateElixir() {
    elixirTimer += 0.2f;
    if (elixirTimer >= ELIXIR_REGEN_RATE) {
        elixirTimer = 0;
        if (elixirPlayerOne < MAX_ELIXIR) {
            elixirPlayerOne = std::min(MAX_ELIXIR, elixirPlayerOne + 1.0f);
        }
        if (elixirPlayerTwo < MAX_ELIXIR) {
            elixirPlayerTwo = std::min(MAX_ELIXIR, elixirPlayerTwo + 1.0f);
        }
    }
}

void Game::render() {
    // Slow down rendering to about 2.5 FPS to make it easier to see
    renderCounter++;
    if (renderCounter % 2 == 0) { 
        renderer.clear();
        renderer.drawBoard(board);
        // The 'isPlayerOneTurn' parameter for drawStatus is no longer relevant, but we pass true as a placeholder
        renderer.drawStatus(elixirPlayerOne, elixirPlayerTwo, true, gameTimer);
        renderer.display();
    }
}

void Game::spawnTroop(char type, int x, bool isPlayerOne) {
    EntityType entityType;
    int health = 0;

    // Randomize spawn position along the width of the board
    x = 5 + (rand() % (Renderer::BOARD_WIDTH - 10)); 
    // Set spawn height based on which player is deploying
    int y = isPlayerOne ? Renderer::BOARD_HEIGHT - 6 : 5;
    
    switch (type) {
        case 'K': entityType = EntityType::KNIGHT; health = 600; break;
        case 'G': entityType = EntityType::GOLEM; health = 500; break;
        case 'P': entityType = EntityType::PEKKA; health = 600; break;
        case 'B': entityType = EntityType::GOBLINS; health = 200; break;
        case 'D': entityType = EntityType::DRAGON; health = 600; break;
        case 'W': entityType = EntityType::WIZARD; health = 500; break;
        case 'A': entityType = EntityType::ARCHERS; health = 120; break;
        case 'C': entityType = EntityType::CANON; health = 500; y = isPlayerOne ? Renderer::BOARD_HEIGHT - 8 : 7; break; // Canons spawn further back
        default: return;
    }
    
    board.addEntity(std::make_shared<Entity>(entityType, x, y, isPlayerOne, health));
}

void Game::handleCombat() {
    board.handleCombat();
}