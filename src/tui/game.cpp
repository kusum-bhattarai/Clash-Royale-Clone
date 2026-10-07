#include "clash_royale/tui/game.hpp"

#include <chrono>
#include <optional>
#include <thread>

namespace cr {
namespace {

/// Maps a keypress to the unit it deploys.
std::optional<EntityType> troopForKey(char key) {
    switch (key) {
        case 'k': return EntityType::KNIGHT;
        case 'g': return EntityType::GOLEM;
        case 'p': return EntityType::PEKKA;
        case 'b': return EntityType::GOBLINS;
        case 'd': return EntityType::DRAGON;
        case 'w': return EntityType::WIZARD;
        case 'a': return EntityType::ARCHERS;
        case 'c': return EntityType::CANON;
        default:  return std::nullopt;
    }
}

}  // namespace

Game::Game(MatchConfig config) : m_sim(config) {}

void Game::run() {
    while (m_sim.isRunning() && !m_quitRequested) {
        processInput();
        m_ai.update(m_sim, /*isPlayerOne=*/false, kDefaultTimeStep);
        m_sim.step(kDefaultTimeStep);
        render();

        if (!m_sim.isRunning() || m_quitRequested) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    renderFinalFrame();
}

void Game::processInput() {
    const auto key = m_input.getInput();
    if (!key.has_value()) {
        return;
    }

    if (*key == 'q') {
        m_quitRequested = true;
        return;
    }

    if (m_state == GameState::SELECTING_LANE) {
        if (*key == 'x') {
            m_state = GameState::SELECTING_TROOP;
            return;
        }

        Lane lane;
        if (*key == 'l') {
            lane = Lane::LEFT;
        } else if (*key == 'r') {
            lane = Lane::RIGHT;
        } else {
            return;  // ignore anything that is not a lane choice
        }

        // deploy() checks affordability and spends the elixir itself, so the
        // cost no longer has to be looked up here. It was previously computed
        // by a second switch statement that could drift from the first.
        m_sim.deploy(m_pendingTroop, lane, /*isPlayerOne=*/true);
        m_state = GameState::SELECTING_TROOP;
        return;
    }

    const auto troop = troopForKey(*key);
    if (troop && m_sim.canAfford(*troop, /*isPlayerOne=*/true)) {
        m_pendingTroop = *troop;
        m_state = GameState::SELECTING_LANE;
    }
}

void Game::render() {
    m_renderer.clear();
    m_renderer.drawBoard(m_sim.board());
    m_renderer.drawStatus(m_sim.elixir(true), m_sim.elixir(false), m_sim.elapsed());
    if (m_state == GameState::SELECTING_LANE) {
        m_renderer.drawPrompt("SELECT LANE: (L)eft or (R)ight. (X) to cancel.");
    }
    m_renderer.display();
}

void Game::renderFinalFrame() {
    m_renderer.clear();
    m_renderer.drawBoard(m_sim.board());
    m_renderer.drawStatus(m_sim.elixir(true), m_sim.elixir(false), m_sim.elapsed());
    m_renderer.drawPrompt(outcomeMessage());
    m_renderer.display();
}

std::string Game::outcomeMessage() const {
    switch (m_sim.result()) {
        case MatchResult::PLAYER_ONE_WINS:
            return "Game Over! Winner: Player 1 (You)!";
        case MatchResult::PLAYER_TWO_WINS:
            return "Game Over! Winner: Player 2 (AI)!";
        case MatchResult::DRAW:
            return "Game Over! It's a draw!";
        case MatchResult::IN_PROGRESS:
            break;
    }

    // Quit mid-match: report who was ahead on tower health, as before.
    const int one = m_sim.towerHealth(true);
    const int two = m_sim.towerHealth(false);
    if (one > two) {
        return "Match abandoned. Player 1 (You) was ahead.";
    }
    if (two > one) {
        return "Match abandoned. Player 2 (AI) was ahead.";
    }
    return "Match abandoned. Scores were level.";
}

}  // namespace cr
