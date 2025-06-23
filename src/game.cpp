#include "game.hpp"
#include <thread>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include "KingTower.hpp"
#include "QueenTower.hpp"
#include "Canon.hpp"
#include "Knight.hpp"

Game::Game() : isRunning(true), elixirPlayerOne(5.0f), elixirPlayerTwo(5.0f), 
               elixirTimer(0.0f), gameTimer(0.0f), renderCounter(0), 
               currentGameState(GameState::SELECTING_TROOP) {
    std::srand(std::time(nullptr));

    int centerX = 19;
    int sideOffset = 12;
    int p2_king_y = 3, p2_queen_y = 5;
    int p1_king_y = 27, p1_queen_y = 25;
    
    // Initialize Player 2 (AI) towers at the top
    board.addEntity(std::make_shared<KingTower>(EntityType::KING_TOWER, centerX, p2_king_y, false, 4000, Lane::LEFT));
    board.addEntity(std::make_shared<QueenTower>(EntityType::QUEEN_TOWER, centerX - 1 - sideOffset, p2_queen_y, false, 1500, Lane::LEFT));
    board.addEntity(std::make_shared<QueenTower>(EntityType::QUEEN_TOWER, centerX + sideOffset, p2_queen_y, false, 1500, Lane::RIGHT));
    
    // Initialize Player 1 (User) towers at the bottom
    board.addEntity(std::make_shared<KingTower>(EntityType::KING_TOWER, centerX, p1_king_y, true, 4000, Lane::LEFT));
    board.addEntity(std::make_shared<QueenTower>(EntityType::QUEEN_TOWER, centerX - 1 - sideOffset, p1_queen_y, true, 1500, Lane::LEFT));
    board.addEntity(std::make_shared<QueenTower>(EntityType::QUEEN_TOWER, centerX + sideOffset, p1_queen_y, true, 1500, Lane::RIGHT));
}

void Game::run() {
    while (isRunning) {
        processInput(); // Handle user input
        update();       // Update game state, including AI moves
        render();       // Draw the result

        if (!isRunning) {
            // Clear the prompt to avoid clutter
            renderer.clear();
            renderer.drawBoard(board);
            renderer.drawStatus(elixirPlayerOne, elixirPlayerTwo, true, gameTimer);
            // Display end message as a prompt
            std::string endMessage;
            int p1Health = 0, p2Health = 0;
            for (const auto& entity : board.getEntities()) {
                if (entity->getType() == EntityType::KING_TOWER || 
                    entity->getType() == EntityType::QUEEN_TOWER) {
                    if (entity->getIsPlayer()) { 
                        p1Health += entity->getHealth();
                    } else {
                        p2Health += entity->getHealth();
                    }
                }
            }
            if (p1Health > p2Health) {
                endMessage = "Game Over! Winner: Player 1 (You)!";
            } else if (p2Health > p1Health) {
                endMessage = "Game Over! Winner: Player 2 (AI)!";
            } else {
                endMessage = "Game Over! It's a draw!";
            }
            renderer.drawPrompt(endMessage);
            renderer.display();
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Game speed
    }
}

void Game::processInput() {
    auto input = inputHandler.getInput();
    if (!input.has_value()) return;

    if (currentGameState == GameState::SELECTING_LANE && (*input == 'x' || *input == 'q')) {
        currentGameState = GameState::SELECTING_TROOP;
        if(*input == 'q') isRunning = false;
        return;
    }

    if (currentGameState == GameState::SELECTING_TROOP) {
        if (*input == 'q') {
            isRunning = false;
            return;
        }

        EntityType selectedType;
        float cost = 0;

        switch (*input) {
            case 'k': selectedType = EntityType::KNIGHT; cost = 4.0f; break;
            case 'g': selectedType = EntityType::GOLEM; cost = 5.0f; break;
            case 'p': selectedType = EntityType::PEKKA; cost = 4.0f; break;
            case 'b': selectedType = EntityType::GOBLINS; cost = 3.0f; break;
            case 'd': selectedType = EntityType::DRAGON; cost = 5.0f; break;
            case 'w': selectedType = EntityType::WIZARD; cost = 4.0f; break;
            case 'a': selectedType = EntityType::ARCHERS; cost = 2.0f; break;
            case 'c': selectedType = EntityType::CANON; cost = 3.0f; break;
            default: return;
        }

        if (elixirPlayerOne >= cost) {
            pendingTroopType = selectedType;
            currentGameState = GameState::SELECTING_LANE;
        }

    } else if (currentGameState == GameState::SELECTING_LANE) {
        Lane selectedLane;
        if (*input == 'l') {
            selectedLane = Lane::LEFT;
        } else if (*input == 'r') {
            selectedLane = Lane::RIGHT;
        } else {
            return;
        }
        
        float cost = 0;
        switch (pendingTroopType) {
            case EntityType::KNIGHT: cost = 4.0f; break;
            case EntityType::GOLEM: cost = 5.0f; break;
            case EntityType::PEKKA: cost = 4.0f; break;
            case EntityType::GOBLINS: cost = 3.0f; break;
            case EntityType::DRAGON: cost = 5.0f; break;
            case EntityType::WIZARD: cost = 4.0f; break;
            case EntityType::ARCHERS: cost = 2.0f; break;
            case EntityType::CANON: cost = 3.0f; break;
            default: break;
        }

        if (elixirPlayerOne >= cost) {
            spawnTroop(pendingTroopType, selectedLane, true);
            elixirPlayerOne -= cost;
        }
        
        currentGameState = GameState::SELECTING_TROOP;
    }
}

void Game::runAI() {
    if (elixirPlayerTwo >= 3.0f) { 
        bool shouldDeploy = (rand() % 10) < 3; 
        if (shouldDeploy) {
            EntityType type;
            float cost;
            int choice = rand() % 8;
            switch(choice) {
                case 0: type = EntityType::KNIGHT; cost = 4.0f; break;
                case 1: type = EntityType::GOLEM; cost = 5.0f; break;
                case 2: type = EntityType::PEKKA; cost = 4.0f; break;
                case 3: type = EntityType::GOBLINS; cost = 3.0f; break;
                case 4: type = EntityType::DRAGON; cost = 5.0f; break;
                case 5: type = EntityType::WIZARD; cost = 4.0f; break;
                case 6: type = EntityType::ARCHERS; cost = 2.0f; break;
                default: type = EntityType::CANON; cost = 3.0f; break;
            }
            
            if (elixirPlayerTwo >= cost) {
                Lane aiLane = (rand() % 2 == 0) ? Lane::LEFT : Lane::RIGHT;
                spawnTroop(type, aiLane, false);
                elixirPlayerTwo -= cost;
            }
        }
    }
}

void Game::spawnTroop(EntityType type, Lane lane, bool isPlayerOne) {
    std::shared_ptr<Entity> newTroop = nullptr;
    int health = 0;
    
    int spawnX = 0;
    if (lane == Lane::LEFT) {
        spawnX = Renderer::BOARD_WIDTH / 4;
    } else { // Lane::RIGHT
        spawnX = Renderer::BOARD_WIDTH * 3 / 4;
    }

    int spawnY = isPlayerOne ? (Renderer::BOARD_HEIGHT / 2) + 3 : (Renderer::BOARD_HEIGHT / 2) - 3;
    
    switch (type) {
        case EntityType::CANON:
            health = 500;
            spawnY = isPlayerOne ? spawnY - 2 : spawnY + 2;
            newTroop = std::make_shared<Canon>(type, spawnX, spawnY, isPlayerOne, health, lane);
            break;
        case EntityType::KNIGHT:
            health = 600;
            newTroop = std::make_shared<Knight>(type, spawnX, spawnY, isPlayerOne, health, lane);
            break;
        default:
            // rest of the troops disabled to avoid compile errors.
            return;
    }

    if (newTroop) {
        board.addEntity(newTroop);
    }
}

void Game::render() {
    renderCounter++;
    renderer.clear();
    renderer.drawBoard(board);
    renderer.drawStatus(elixirPlayerOne, elixirPlayerTwo, true, gameTimer);
    if(currentGameState == GameState::SELECTING_LANE) {
        renderer.drawPrompt("SELECT LANE: (L)eft or (R)ight. (X) to cancel.");
    }
    renderer.display();
}

void Game::update() {
    gameTimer += 0.1f;
    if (gameTimer >= GAME_DURATION) {
        int p1Health = 0, p2Health = 0;
        for (const auto& entity : board.getEntities()) {
            if (entity->getType() == EntityType::KING_TOWER || 
                entity->getType() == EntityType::QUEEN_TOWER) {
                if (entity->getIsPlayer()) { 
                    p1Health += entity->getHealth();
                } else {
                    p2Health += entity->getHealth();
                }
            }
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
    elixirTimer += 0.1f;
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

void Game::handleCombat() {
    board.handleCombat();
}