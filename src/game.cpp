#include "game.hpp"
#include <thread>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <fcntl.h>

Game::Game() : isRunning(true), elixirPlayerOne(5.0f), elixirPlayerTwo(5.0f), 
               elixirTimer(0.0f), isPlayerOneTurn(true), gameTimer(0.0f) {
    std::srand(std::time(nullptr));
    Board::setCurrentBoard(&board);
    
    // Initialize Player 1 towers (top)
    board.addEntity(std::make_shared<Entity>(EntityType::KING_TOWER, 13, 2, true, 4000));
    board.addEntity(std::make_shared<Entity>(EntityType::QUEEN_TOWER, 6, 2, true, 1500));
    board.addEntity(std::make_shared<Entity>(EntityType::QUEEN_TOWER, 20, 2, true, 1500));
    
    // Initialize Player 2 towers (bottom)
    board.addEntity(std::make_shared<Entity>(EntityType::KING_TOWER, 13, 27, false, 4000));
    board.addEntity(std::make_shared<Entity>(EntityType::QUEEN_TOWER, 6, 27, false, 1500));
    board.addEntity(std::make_shared<Entity>(EntityType::QUEEN_TOWER, 20, 27, false, 1500));
}

void Game::run() {
    while (isRunning) {
        processInput();
        update();
        render();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void Game::processInput() {
    static struct termios old_tio;
    static struct termios new_tio;
    static bool initialized = false;
    
    if (!initialized) {
        tcgetattr(STDIN_FILENO, &old_tio);
        new_tio = old_tio;
        new_tio.c_lflag &= (~ICANON & ~ECHO);
        new_tio.c_cc[VMIN] = 0;
        new_tio.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
        fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
        initialized = true;
    }
    
    fd_set rfds;
    struct timeval tv;
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);
    tv.tv_sec = 0;
    tv.tv_usec = 100000;
    
    char input;
    if (select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv) > 0 && read(STDIN_FILENO, &input, 1) > 0) {
        tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);

        input = std::tolower(input);
        switch (input) {
            case 'q':
                isRunning = false;
                return;
            case 'r':
                // Reset game state
                elixirPlayerOne = 5.0f;
                elixirPlayerTwo = 5.0f;
                elixirTimer = 0.0f;
                isPlayerOneTurn = true;
                gameTimer = 0.0f;
                board = Board();
                Board::setCurrentBoard(&board);
                // Reinitialize towers
                board.addEntity(std::make_shared<Entity>(EntityType::KING_TOWER, 13, 2, true, 4000));
                board.addEntity(std::make_shared<Entity>(EntityType::QUEEN_TOWER, 6, 2, true, 1500));
                board.addEntity(std::make_shared<Entity>(EntityType::QUEEN_TOWER, 20, 2, true, 1500));
                board.addEntity(std::make_shared<Entity>(EntityType::KING_TOWER, 13, 27, false, 4000));
                board.addEntity(std::make_shared<Entity>(EntityType::QUEEN_TOWER, 6, 27, false, 1500));
                board.addEntity(std::make_shared<Entity>(EntityType::QUEEN_TOWER, 20, 27, false, 1500));
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
                    float& currentElixir = isPlayerOneTurn ? elixirPlayerOne : elixirPlayerTwo;
                    float cost = (input == 'g' || input == 'd') ? 5.0f :
                               (input == 'a') ? 2.0f :
                               (input == 'b' || input == 'c') ? 3.0f : 4.0f;
                    
                    if (currentElixir >= cost) {
                        spawnTroop(std::toupper(input), isPlayerOneTurn ? 15 : 15, isPlayerOneTurn);
                        currentElixir -= cost;
                        switchTurns();
                    }
                }
                break;
        }
    }
}

void Game::update() {
    gameTimer += 0.1f;
    if (gameTimer >= GAME_DURATION) {
        int p1Damage = 0, p2Damage = 0;
        for (const auto& entity : board.getEntities()) {
            if ((entity->getType() == EntityType::KING_TOWER || 
                 entity->getType() == EntityType::QUEEN_TOWER)) {
                if (entity->getIsPlayer()) {
                    p1Damage += (entity->getType() == EntityType::KING_TOWER ? 4000 : 1500) - entity->getHealth();
                } else {
                    p2Damage += (entity->getType() == EntityType::KING_TOWER ? 4000 : 1500) - entity->getHealth();
                }
            }
        }
        std::cout << "\nGame Over!\n";
        std::cout << "Player 1 damage taken: " << p1Damage << "\n";
        std::cout << "Player 2 damage taken: " << p2Damage << "\n";
        std::cout << "Winner: Player " << (p1Damage < p2Damage ? "1" : "2") << "!\n";
        isRunning = false;
        return;
    }
    updateElixir();
    board.updateEntities();
    handleCombat();
}

void Game::updateElixir() {
    elixirTimer += 0.1f;
    if (elixirTimer >= ELIXIR_REGEN_RATE) {
        elixirTimer = 0;
        if (elixirPlayerOne < MAX_ELIXIR) {
            elixirPlayerOne += 1.0f;
        }
        if (elixirPlayerTwo < MAX_ELIXIR) {
            elixirPlayerTwo += 1.0f;
        }
    }
}

void Game::switchTurns() {
    isPlayerOneTurn = !isPlayerOneTurn;
}

void Game::render() {
    renderer.clear();
    renderer.drawBoard(board);
    renderer.drawStatus(elixirPlayerOne, elixirPlayerTwo, isPlayerOneTurn, gameTimer);
    renderer.display();
}

void Game::spawnTroop(char type, int x, bool isPlayerOne) {
    EntityType entityType;
    int health = 0;
    // Randomize x position within reasonable bounds for each player's side
    x = 5 + (rand() % 20);  // Random position between x=5 and x=25
    int y = isPlayerOne ? 3 : 27;  // Troops spawn closer to their respective sides
    
    switch (type) {
        case 'K':
            entityType = EntityType::KNIGHT;
            health = 600;
            break;
        case 'G':
            entityType = EntityType::GOLEM;
            health = 500;
            break;
        case 'P':
            entityType = EntityType::PEKKA;
            health = 600;
            break;
        case 'B':
            entityType = EntityType::GOBLINS;
            health = 200;
            break;
        case 'D':
            entityType = EntityType::DRAGON;
            health = 600;
            break;
        case 'W':
            entityType = EntityType::WIZARD;
            health = 500;
            break;
        case 'A':
            entityType = EntityType::ARCHERS;
            health = 120;
            break;
        case 'C':
            entityType = EntityType::CANON;
            health = 500;
            break;
        default:
            return;
    }
    
    board.addEntity(std::make_shared<Entity>(entityType, x, y, isPlayerOne, health));
}

void Game::handleCombat() {
    board.handleCombat();
}

const float ELIXIR_REGEN_RATE = 1.8f;
const float MAX_ELIXIR = 10.0f;