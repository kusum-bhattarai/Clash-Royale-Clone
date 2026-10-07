#pragma once

namespace cr {

class Simulation;

/// Decides what a non-human player deploys and when.
///
/// Implement this to plug in your own opponent. A controller is called once per
/// simulation step and acts purely through the public Simulation interface --
/// reading the board and elixir, then calling `deploy()` -- so it has no
/// privileged access and cannot cheat.
class AiController {
public:
    virtual ~AiController() = default;

    /// Called once per step, before the simulation advances.
    ///
    /// \param sim          the match in progress
    /// \param isPlayerOne  which side this controller is playing
    /// \param dt           seconds this step covers
    virtual void update(Simulation& sim, bool isPlayerOne, float dt) = 0;
};

}  // namespace cr
