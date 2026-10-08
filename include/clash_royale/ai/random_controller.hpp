#pragma once

#include "clash_royale/ai/controller.hpp"

namespace cr {

/// The default opponent: deploys a random affordable unit down a random lane.
///
/// It reacts to nothing -- not the player's pushes, not its own elixir
/// advantage, not which of its towers is under threat. It is the behavior the
/// game shipped with, kept as a baseline and as the simplest possible example
/// of the AiController interface.
class RandomAiController : public AiController {
public:
    /// \param deployChancePerStep  probability of attempting a deploy on any
    ///        given step, once the minimum elixir is available
    /// \param minimumElixir        elixir below which it will not act
    explicit RandomAiController(float deployChancePerStep = 0.3f, float minimumElixir = 3.0f)
        : m_deployChance(deployChancePerStep), m_minimumElixir(minimumElixir) {}

    void update(Simulation& sim, bool isPlayerOne, float dt) override;

private:
    float m_deployChance;
    float m_minimumElixir;
};

}  // namespace cr
