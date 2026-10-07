#pragma once

#include <string>

#include "clash_royale/ai/random_controller.hpp"
#include "clash_royale/sim/simulation.hpp"
#include "clash_royale/tui/input_handler.hpp"
#include "clash_royale/tui/renderer.hpp"

namespace cr {

/// Which half of the two-step deploy the player is in.
enum class GameState {
    SELECTING_TROOP,
    SELECTING_LANE,
};

/// The playable terminal game.
///
/// This is a front-end and nothing more: it owns a Simulation, polls the
/// keyboard, draws frames and paces itself with a sleep. All match rules --
/// elixir, combat, the clock, the win condition -- live in Simulation, which
/// knows nothing about terminals and can be driven without one.
///
/// Game used to hold that state itself and exposed five protected virtuals
/// purely so a test subclass could reach them. Those are gone: the logic they
/// guarded is now directly testable through Simulation's public interface.
class Game {
public:
    explicit Game(MatchConfig config = {});

    /// Runs the match to completion, or until the player quits.
    void run();

    Simulation& simulation() { return m_sim; }
    const Simulation& simulation() const { return m_sim; }

private:
    void processInput();
    void render();
    void renderFinalFrame();
    std::string outcomeMessage() const;

    Simulation m_sim;
    RandomAiController m_ai;
    Renderer m_renderer;
    InputHandler m_input;
    GameState m_state = GameState::SELECTING_TROOP;
    std::string m_pendingCard;
    bool m_quitRequested = false;
};

}  // namespace cr
