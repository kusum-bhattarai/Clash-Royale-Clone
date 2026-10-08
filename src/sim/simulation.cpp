#include "clash_royale/sim/simulation.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "clash_royale/sim/default_cards.hpp"
#include "clash_royale/sim/entity.hpp"

namespace cr {
namespace {

/// Tower placement, mirrored about both arena axes.
///
/// The original layout was asymmetric in two ways, and both measurably favoured
/// one player. Vertically, player two's towers sat at rows 3 and 5 -- 14 and 12
/// tiles from the midline -- while player one's sat at 27 and 25, only 10 and 8
/// away, so player one's towers were four tiles more exposed. Horizontally, the
/// left queen towers were placed one tile further out than the right ones.
///
/// Over 400 seeded matches between two identical random opponents, player one
/// won 54% even when moving second, which is the asymmetry showing through.
/// Towers are now mirrored about x = kCenterX and about the river row, so a
/// match between equal opponents is decided by play rather than by side.
///
/// kCenterX is the arena's horizontal middle and the axis the bridges are
/// already symmetric about. One residual remains: the playable interior spans
/// x in [1, width - 2], whose true centre is half a tile left of kCenterX for an
/// even width. That half tile is inherent to an even-width board.
constexpr int kCenterX = kArenaWidth / 2;              // 20
constexpr int kSideOffset = 12;
constexpr int kRiverRow = kArenaHeight / 2;            // 17
constexpr int kKingSetback = 10;
constexpr int kQueenSetback = 8;

constexpr int kPlayerTwoKingY = kRiverRow - kKingSetback;    // 7
constexpr int kPlayerTwoQueenY = kRiverRow - kQueenSetback;  // 9
constexpr int kPlayerOneKingY = kRiverRow + kKingSetback;    // 27
constexpr int kPlayerOneQueenY = kRiverRow + kQueenSetback;  // 25

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
      m_board(m_config.arena),
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
    m_board.spawn(king, kCenterX, kPlayerTwoKingY, false, Lane::LEFT);
    m_board.spawn(queen, kCenterX - kSideOffset, kPlayerTwoQueenY, false, Lane::LEFT);
    m_board.spawn(queen, kCenterX + kSideOffset, kPlayerTwoQueenY, false, Lane::RIGHT);

    m_board.spawn(king, kCenterX, kPlayerOneKingY, true, Lane::LEFT);
    m_board.spawn(queen, kCenterX - kSideOffset, kPlayerOneQueenY, true, Lane::LEFT);
    m_board.spawn(queen, kCenterX + kSideOffset, kPlayerOneQueenY, true, Lane::RIGHT);
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
    m_board.spawn(spec, spawn.x, spawn.y, isPlayerOne, lane);

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
