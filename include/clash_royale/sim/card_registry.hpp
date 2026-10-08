#pragma once

#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "clash_royale/sim/card.hpp"

namespace cr {

class Arena;
class Entity;

/// A collection of card definitions, keyed by id.
///
/// Registered specs are stored at stable addresses, so a `const CardSpec&`
/// handed out by the registry stays valid as more cards are defined. Entities
/// hold such a reference rather than copying stats, which is what lets a card's
/// numbers be read straight from data in the hot loop.
class CardRegistry {
public:
    /// An empty registry. Most callers want withDefaultCards() instead.
    CardRegistry() = default;

    /// The ten cards the game ships with.
    static CardRegistry withDefaultCards();

    /// Registers a card, replacing any existing one with the same id.
    ///
    /// Returns a reference that remains valid for the registry's lifetime.
    const CardSpec& define(CardSpec spec);

    /// Looks a card up, or returns nullptr when the id is unknown.
    const CardSpec* find(std::string_view id) const;

    /// Looks a card up, throwing std::out_of_range when the id is unknown.
    const CardSpec& get(std::string_view id) const;

    bool contains(std::string_view id) const { return find(id) != nullptr; }
    std::size_t size() const { return m_index.size(); }

    /// Every card a player can deploy, in registration order.
    ///
    /// Maintained as cards are defined rather than rebuilt per call, since an
    /// AI controller reads this on every simulation step.
    ///
    /// \warning The returned reference is invalidated by define(). Copy it
    /// before registering further cards -- the CardSpec pointers inside stay
    /// valid, but the vector holding them does not.
    const std::vector<const CardSpec*>& deployable() const { return m_deployable; }

    /// Every registered card, in registration order.
    ///
    /// \warning Invalidated by define(), as deployable() is.
    const std::vector<const CardSpec*>& all() const { return m_all; }

private:
    void rebuildViews();

    // A deque never reallocates existing elements, so references handed out by
    // define() survive later registrations. A vector would not.
    std::deque<CardSpec> m_storage;
    std::unordered_map<std::string, CardSpec*> m_index;
    std::vector<const CardSpec*> m_all;
    std::vector<const CardSpec*> m_deployable;
};

/// Creates an entity from a card, positioned on `arena`.
///
/// The arena is used to clamp the starting position; the entity does not retain
/// it, and takes the arena it moves on from whichever Board holds it.
///
/// Honours `CardSpec::factory` when set, so a card may supply its own Entity
/// subclass; otherwise builds a plain spec-driven Entity.
std::shared_ptr<Entity> createEntity(const CardSpec& spec, const Arena& arena, int x, int y, bool isPlayer,
                                     Lane lane);

}  // namespace cr
