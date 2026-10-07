#pragma once

#include <cstdint>

#include <string>
#include <string_view>

#include "clash_royale/core/rng.hpp"
#include "clash_royale/core/types.hpp"
#include "clash_royale/sim/board.hpp"
#include "clash_royale/sim/card_registry.hpp"
#include "clash_royale/sim/default_cards.hpp"

namespace cr {

/// How a match ended, or that it has not.
enum class MatchResult {
    IN_PROGRESS,
    PLAYER_ONE_WINS,
    PLAYER_TWO_WINS,
    DRAW,
};

/// Match parameters. Exposed so downstream users can run shorter matches,
/// change the elixir economy, or pin a seed for a reproducible replay.
struct MatchConfig {
    float matchDuration = 120.0f;       ///< seconds before time runs out
    float elixirRegenInterval = 2.8f;   ///< seconds per point of elixir
    float maxElixir = 10.0f;
    float startingElixir = 5.0f;
    CombatRules combat{};

    /// The cards available in this match. Replace or extend this to play with
    /// a custom roster; entities hold references into it, so the simulation
    /// keeps its own copy for the duration of the match.
    CardRegistry cards = CardRegistry::withDefaultCards();

    /// Which cards are placed as the starting towers.
    ///
    /// A match needs both, because the win condition is defined in terms of
    /// them. A custom roster either defines cards under these ids or points
    /// these at its own equivalents.
    std::string kingTowerCard{cards::KingTower};
    std::string queenTowerCard{cards::QueenTower};

    /// Seed for the simulation's generator. When nullopt, a seed is drawn from
    /// system entropy; either way `Simulation::rng().seed()` reports it.
    bool deterministic = false;
    std::uint64_t seed = 0;
};

/// Where a unit appears when deployed down a lane.
struct SpawnPoint {
    int x;
    int y;
};
SpawnPoint spawnPointFor(const CardSpec& spec, Lane lane, bool isPlayerOne);

/// A headless match.
///
/// Holds everything about a game in progress except how it is presented or
/// controlled: the board, both players' elixir, the clock, the outcome and the
/// random generator. It performs no I/O and has no platform dependencies, so it
/// can be driven by a terminal front-end, a test, a bot or a training harness
/// at whatever rate the caller likes.
class Simulation {
public:
    /// Throws std::invalid_argument when the configured registry does not
    /// define the two tower cards the win condition depends on.
    explicit Simulation(MatchConfig config = {});

    /// Advances the match by `dt` seconds: elixir regeneration, entity
    /// movement, combat, then the win condition.
    ///
    /// Does nothing once the match has ended.
    void step(float dt = kDefaultTimeStep);

    bool isRunning() const { return m_result == MatchResult::IN_PROGRESS; }
    MatchResult result() const { return m_result; }
    float elapsed() const { return m_elapsed; }
    const MatchConfig& config() const { return m_config; }

    float elixir(bool isPlayerOne) const { return isPlayerOne ? m_elixirOne : m_elixirTwo; }

    /// The cards available in this match.
    const CardRegistry& cards() const { return m_config.cards; }

    /// True when the player can afford the named card.
    /// Unknown ids and non-deployable cards return false.
    bool canAfford(std::string_view cardId, bool isPlayerOne) const;

    /// Deploys a card down a lane, spending its elixir cost.
    ///
    /// Returns false and changes nothing when the id is unknown, the card is
    /// not deployable, the player cannot afford it, or the match has ended --
    /// so callers never have to pre-check.
    bool deploy(std::string_view cardId, Lane lane, bool isPlayerOne);

    /// Total remaining health across a player's towers. This is the tiebreaker
    /// when a match reaches full time.
    int towerHealth(bool isPlayerOne) const;

    Board& board() { return m_board; }
    const Board& board() const { return m_board; }
    Rng& rng() { return m_rng; }

private:
    void regenerateElixir(float dt);
    void evaluateResult();

    MatchConfig m_config;
    Rng m_rng;
    Board m_board;
    float m_elixirOne;
    float m_elixirTwo;
    float m_elixirTimer = 0.0f;
    float m_elapsed = 0.0f;
    MatchResult m_result = MatchResult::IN_PROGRESS;
};

}  // namespace cr
