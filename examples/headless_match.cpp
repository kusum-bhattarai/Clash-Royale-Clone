// Runs a full match with no renderer, no terminal and no game loop of its own.
//
// This is the shape of using the library for anything that is not the shipped
// game: a bot, a balance sweep, a training harness. Simulation performs no I/O,
// so it runs far faster than real time -- a 120-second match takes a couple of
// milliseconds.
//
// Build with -DCR_BUILD_EXAMPLES=ON.

#include <cstdio>

#include "clash_royale/ai/random_controller.hpp"
#include "clash_royale/ai/strategic_controller.hpp"
#include "clash_royale/sim/simulation.hpp"

int main() {
    cr::MatchConfig config;
    config.deterministic = true;  // same seed replays the same match, everywhere
    config.seed = 2024;

    cr::Simulation sim{config};
    cr::StrategicAiController playerOne;
    cr::RandomAiController playerTwo;

    while (sim.isRunning()) {
        playerOne.update(sim, /*isPlayerOne=*/true, cr::kDefaultTimeStep);
        playerTwo.update(sim, /*isPlayerOne=*/false, cr::kDefaultTimeStep);
        sim.step();  // or sim.step(1.0f) to run coarser and faster
    }

    const char* outcome = "draw";
    switch (sim.result()) {
        case cr::MatchResult::PLAYER_ONE_WINS: outcome = "player one"; break;
        case cr::MatchResult::PLAYER_TWO_WINS: outcome = "player two"; break;
        default: break;
    }

    std::printf("seed %llu: %s after %.0fs\n", static_cast<unsigned long long>(sim.rng().seed()), outcome,
                static_cast<double>(sim.elapsed()));
    std::printf("tower health: P1 %d, P2 %d\n", sim.towerHealth(true), sim.towerHealth(false));
    std::printf("P1 decisions: %zu defensive, %zu offensive\n", playerOne.telemetry().defensiveDeploys,
                playerOne.telemetry().offensiveDeploys);
    return 0;
}
