#include "clash_royale/ai/random_controller.hpp"

#include <vector>

#include "clash_royale/sim/card.hpp"
#include "clash_royale/sim/simulation.hpp"

namespace cr {

void RandomAiController::update(Simulation& sim, bool isPlayerOne, float /*dt*/) {
    if (sim.elixir(isPlayerOne) < m_minimumElixir) {
        return;
    }
    if (!sim.rng().chance(m_deployChance)) {
        return;
    }

    // The roster comes from the registry rather than a hardcoded list, so a
    // custom card added by a downstream project is picked up automatically.
    const std::vector<const CardSpec*>& options = sim.cards().deployable();
    if (options.empty()) {
        return;
    }

    const CardSpec& pick = *options[sim.rng().below(static_cast<std::uint32_t>(options.size()))];
    const Lane lane = (sim.rng().below(2) == 0) ? Lane::LEFT : Lane::RIGHT;

    // deploy() re-checks affordability, so an unaffordable pick simply does
    // nothing this step rather than needing a guard here.
    sim.deploy(pick.id, lane, isPlayerOne);
}

}  // namespace cr
