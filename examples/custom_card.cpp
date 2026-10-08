// Adds a unit the library has never heard of, and plays it.
//
// Nothing here edits the library. A card is a CardSpec value: stats, targeting
// rules and damage modifiers are data, so a downstream project defines its own
// without forking. The default AI picks the new card up automatically, because
// it reads the roster from the registry rather than from a hardcoded list.
//
// Build with -DCR_BUILD_EXAMPLES=ON.

#include <cstdio>

#include "clash_royale/ai/strategic_controller.hpp"
#include "clash_royale/sim/simulation.hpp"

int main() {
    // A fast, fragile flier that savages heavy armour.
    cr::CardSpec harpy;
    harpy.id = "harpy";
    harpy.displayName = "Harpy";
    harpy.symbol = 'H';
    harpy.health = 240;
    harpy.damage = 30;
    harpy.attackRange = 2;
    harpy.moveSpeed = 1.6f;          // tiles per second
    harpy.attackSpeed = 1.0f / 0.9f; // 0.9s hit speed
    harpy.elixirCost = 3.0f;
    harpy.domain = cr::MovementDomain::Air;
    harpy.movement = cr::MovementStyle::Diagonal;
    harpy.armor = cr::ArmorClass::Light;
    harpy.targets = cr::TargetFilter{/*ground=*/true, /*air=*/true};

    // Double damage against heavy armour. Conditions may also match a movement
    // domain, specific card ids, or whether the target is a building; an empty
    // condition matches anything.
    harpy.damageModifiers = {
        cr::DamageModifier{.multiplier = 2.0f, .againstArmor = {cr::ArmorClass::Heavy}},
    };

    cr::MatchConfig config;
    config.deterministic = true;
    config.seed = 7;
    config.cards.define(harpy);

    cr::Simulation sim{config};
    std::printf("roster: %zu cards, %zu of them deployable\n", sim.cards().size(),
                sim.cards().deployable().size());

    cr::StrategicAiController opponent;
    int played = 0;
    while (sim.isRunning()) {
        opponent.update(sim, /*isPlayerOne=*/false, cr::kDefaultTimeStep);
        if (sim.deploy("harpy", cr::Lane::LEFT, /*isPlayerOne=*/true)) {
            ++played;
        }
        sim.step();
    }

    std::printf("played %d harpies; final tower health P1 %d, P2 %d\n", played, sim.towerHealth(true),
                sim.towerHealth(false));

    // For behaviour that cannot be expressed as data, set CardSpec::factory to
    // return your own cr::Entity subclass and override move(), findTarget() or
    // update() while still declaring the stats here.
    return 0;
}
