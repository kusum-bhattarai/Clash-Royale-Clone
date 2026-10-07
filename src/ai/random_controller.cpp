#include "clash_royale/ai/random_controller.hpp"

#include <array>

#include "clash_royale/sim/simulation.hpp"

namespace cr {
namespace {

/// Every unit a player can actually deploy. Towers are excluded.
constexpr std::array<EntityType, 8> kDeployable{
    EntityType::KNIGHT, EntityType::GOLEM,  EntityType::PEKKA,   EntityType::GOBLINS,
    EntityType::DRAGON, EntityType::WIZARD, EntityType::ARCHERS, EntityType::CANON,
};

}  // namespace

void RandomAiController::update(Simulation& sim, bool isPlayerOne, float /*dt*/) {
    if (sim.elixir(isPlayerOne) < m_minimumElixir) {
        return;
    }
    if (!sim.rng().chance(m_deployChance)) {
        return;
    }

    const EntityType type = kDeployable[sim.rng().below(kDeployable.size())];
    const Lane lane = (sim.rng().below(2) == 0) ? Lane::LEFT : Lane::RIGHT;

    // deploy() re-checks affordability, so an unaffordable pick simply does
    // nothing this step rather than needing a guard here.
    sim.deploy(type, lane, isPlayerOne);
}

}  // namespace cr
