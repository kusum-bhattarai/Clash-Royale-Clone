#include "clash_royale/sim/simulation.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "clash_royale/sim/default_cards.hpp"
#include "clash_royale/sim/entity.hpp"

namespace cr {
namespace {

/// Tower placement. centerX is the arena midline; the queen towers sit
/// `sideOffset` tiles to either side.
///
/// Note the asymmetry: the left queen towers land one tile further from center
/// than the right ones, because of the `- 1 -` below. That is preserved here
/// deliberately -- it is existing behavior that tests pin, and correcting it
/// belongs with the arena work rather than buried in an extraction.
constexpr int kCenterX = 19;
constexpr int kSideOffset = 12;
constexpr int kPlayerTwoKingY = 3;
constexpr int kPlayerTwoQueenY = 5;
constexpr int kPlayerOneKingY = 27;
constexpr int kPlayerOneQueenY = 25;

}  // namespace

SpawnPoint spawnPointFor(const CardSpec& spec, Lane lane, bool isPlayerOne) {
    const int x = (lane == Lane::LEFT) ? kArenaWidth / 4 : kArenaWidth * 3 / 4;
    int y = isPlayerOne ? (kArenaHeight / 2) + 3 : (kArenaHeight / 2) - 3;

    // Cards may be placed further back than the lane spawn line; a Canon sets
    // this to 2. It used to be an `if (type == CANON)` special case here.
    if (spec.spawnSetback != 0) {
        y += isPlayerOne ? -spec.spawnSetback : spec.spawnSetback;
    }
    return SpawnPoint{x, y};
}

Simulation::Simulation(MatchConfig config)
    : m_config(std::move(config)),
      m_rng(m_config.deterministic ? Rng(m_config.seed) : Rng::fromEntropy()),
      m_elixirOne(m_config.startingElixir),
      m_elixirTwo(m_config.startingElixir) {
    m_board.setCombatRules(m_config.combat);

    // Entities hold references into m_config.cards, which lives as long as the
    // simulation, so the specs they point at stay valid and at a fixed address.
    const CardSpec* kingSpec = m_config.cards.find(m_config.kingTowerCard);
    const CardSpec* queenSpec = m_config.cards.find(m_config.queenTowerCard);
    if (kingSpec == nullptr || queenSpec == nullptr) {
        // Fail with something actionable rather than letting a bare
        // out_of_range escape from a registry lookup.
        throw std::invalid_argument(
            "MatchConfig::cards must define the tower cards '" + m_config.kingTowerCard + "' and '" +
            m_config.queenTowerCard +
            "'; a custom roster should either define them or set "
            "MatchConfig::kingTowerCard / queenTowerCard to its own equivalents");
    }
    const CardSpec& king = *kingSpec;
    const CardSpec& queen = *queenSpec;

    // Player two (the AI) occupies the top of the arena, player one the bottom.
    m_board.addEntity(createEntity(king, kCenterX, kPlayerTwoKingY, false, Lane::LEFT));
    m_board.addEntity(createEntity(queen, kCenterX - 1 - kSideOffset, kPlayerTwoQueenY, false, Lane::LEFT));
    m_board.addEntity(createEntity(queen, kCenterX + kSideOffset, kPlayerTwoQueenY, false, Lane::RIGHT));

    m_board.addEntity(createEntity(king, kCenterX, kPlayerOneKingY, true, Lane::LEFT));
    m_board.addEntity(createEntity(queen, kCenterX - 1 - kSideOffset, kPlayerOneQueenY, true, Lane::LEFT));
    m_board.addEntity(createEntity(queen, kCenterX + kSideOffset, kPlayerOneQueenY, true, Lane::RIGHT));
}

void Simulation::step(float dt) {
    if (!isRunning()) {
        return;
    }

    m_elapsed += dt;
    regenerateElixir(dt);
    m_board.updateEntities(dt);
    m_board.handleCombat(m_rng, dt);
    evaluateResult();
}

void Simulation::regenerateElixir(float dt) {
    if (m_config.elixirRegenInterval <= 0.0f) {
        return;
    }

    m_elixirTimer += dt;

    // Subtract the interval rather than zeroing the timer, and loop rather than
    // granting at most once. The original code did neither, which was invisible
    // while the timestep was hardcoded at 0.1s but is wrong as soon as the
    // caller picks dt: a single step(10.0f) would hand out one point of elixir
    // instead of three, and zeroing discarded the remainder so the economy
    // drifted slower than the configured interval.
    while (m_elixirTimer >= m_config.elixirRegenInterval) {
        m_elixirTimer -= m_config.elixirRegenInterval;
        m_elixirOne = std::min(m_config.maxElixir, m_elixirOne + 1.0f);
        m_elixirTwo = std::min(m_config.maxElixir, m_elixirTwo + 1.0f);
    }
}

bool Simulation::canAfford(std::string_view cardId, bool isPlayerOne) const {
    const CardSpec* spec = m_config.cards.find(cardId);
    if (spec == nullptr || !spec->deployable) {
        return false;
    }
    return elixir(isPlayerOne) >= spec->elixirCost;
}

bool Simulation::deploy(std::string_view cardId, Lane lane, bool isPlayerOne) {
    if (!isRunning() || !canAfford(cardId, isPlayerOne)) {
        return false;
    }

    const CardSpec& spec = m_config.cards.get(cardId);
    const SpawnPoint spawn = spawnPointFor(spec, lane, isPlayerOne);
    m_board.addEntity(createEntity(spec, spawn.x, spawn.y, isPlayerOne, lane));

    float& pool = isPlayerOne ? m_elixirOne : m_elixirTwo;
    pool -= spec.elixirCost;
    return true;
}

int Simulation::towerHealth(bool isPlayerOne) const {
    int total = 0;
    for (const auto& entity : m_board.getEntities()) {
        if (entity->spec().towerRole != TowerRole::None && entity->getIsPlayer() == isPlayerOne) {
            total += entity->getHealth();
        }
    }
    return total;
}

void Simulation::evaluateResult() {
    // A destroyed King Tower ends the match immediately. This check existed as
    // Game::checkWinCondition() but was never called from the game loop, so
    // destroying a King Tower did not actually end a game -- play continued
    // until the clock expired.
    bool playerOneKingAlive = false;
    bool playerTwoKingAlive = false;
    for (const auto& entity : m_board.getEntities()) {
        if (entity->spec().towerRole != TowerRole::King) {
            continue;
        }
        if (entity->getIsPlayer()) {
            playerOneKingAlive = entity->isAlive();
        } else {
            playerTwoKingAlive = entity->isAlive();
        }
    }

    if (!playerOneKingAlive || !playerTwoKingAlive) {
        if (playerOneKingAlive) {
            m_result = MatchResult::PLAYER_ONE_WINS;
        } else if (playerTwoKingAlive) {
            m_result = MatchResult::PLAYER_TWO_WINS;
        } else {
            m_result = MatchResult::DRAW;  // both kings fell on the same tick
        }
        return;
    }

    if (m_elapsed >= m_config.matchDuration) {
        const int one = towerHealth(true);
        const int two = towerHealth(false);
        m_result = (one > two)   ? MatchResult::PLAYER_ONE_WINS
                   : (two > one) ? MatchResult::PLAYER_TWO_WINS
                                 : MatchResult::DRAW;
    }
}

}  // namespace cr
