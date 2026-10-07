#pragma once

// Shared fixtures for the test suite.

#include <memory>
#include <string_view>

#include "clash_royale/sim/card_registry.hpp"
#include "clash_royale/sim/default_cards.hpp"
#include "clash_royale/sim/entity.hpp"

namespace cr::testing {

/// The built-in roster, constructed once.
///
/// Entities hold a reference to their spec, so the registry backing them must
/// outlive them; a function-local static is the simplest way to guarantee that
/// for tests.
inline const CardRegistry& defaultCards() {
    static const CardRegistry registry = CardRegistry::withDefaultCards();
    return registry;
}

/// Creates an entity from a built-in card id.
inline std::shared_ptr<Entity> spawn(std::string_view cardId, int x, int y, bool isPlayer,
                                     Lane lane = Lane::LEFT) {
    return createEntity(defaultCards().get(cardId), x, y, isPlayer, lane);
}

}  // namespace cr::testing
