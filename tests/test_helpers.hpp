#pragma once

// Shared fixtures for the test suite.

#include <memory>
#include <string_view>

#include "clash_royale/core/arena.hpp"
#include "clash_royale/sim/card_registry.hpp"
#include "clash_royale/sim/default_cards.hpp"
#include "clash_royale/sim/board.hpp"
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

/// The shipped arena, constructed once.
inline const Arena& standardArena() {
    static const Arena arena = Arena::standard();
    return arena;
}

/// An arena with no terrain features, for tests about movement rather than
/// routing.
inline const Arena& openArena() {
    static const Arena arena{kArenaWidth, kArenaHeight};
    return arena;
}

/// Creates an entity from a built-in card id, positioned on the shipped arena.
inline std::shared_ptr<Entity> spawn(std::string_view cardId, int x, int y, bool isPlayer,
                                     Lane lane = Lane::LEFT) {
    return createEntity(defaultCards().get(cardId), standardArena(), x, y, isPlayer, lane);
}

/// A board with no terrain, so straight-line movement can be observed without
/// the river redirecting units toward a bridge.
inline Board openBoard() {
    return Board{Arena{kArenaWidth, kArenaHeight}};
}

}  // namespace cr::testing
