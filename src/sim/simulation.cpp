#include "clash_royale/sim/simulation.hpp"

#include <algorithm>

#include "clash_royale/sim/entity_factory.hpp"

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

bool isKingOrQueen(EntityType type) {
    return type == EntityType::KING_TOWER || type == EntityType::QUEEN_TOWER;
}

}  // namespace

float elixirCost(EntityType type) {
    switch (type) {
        case EntityType::ARCHERS:
            return 2.0f;
        case EntityType::GOBLINS:
        case EntityType::CANON:
            return 3.0f;
        case EntityType::KNIGHT:
        case EntityType::PEKKA:
        case EntityType::WIZARD:
            return 4.0f;
        case EntityType::GOLEM:
        case EntityType::DRAGON:
            return 5.0f;
        case EntityType::KING_TOWER:
        case EntityType::QUEEN_TOWER:
            return 0.0f;  // not deployable
    }
    return 0.0f;
}

SpawnPoint spawnPointFor(EntityType type, Lane lane, bool isPlayerOne) {
    const int x = (lane == Lane::LEFT) ? kArenaWidth / 4 : kArenaWidth * 3 / 4;
    int y = isPlayerOne ? (kArenaHeight / 2) + 3 : (kArenaHeight / 2) - 3;

    // Buildings are placed a little further back than troops.
    if (type == EntityType::CANON) {
        y = isPlayerOne ? y - 2 : y + 2;
    }
    return SpawnPoint{x, y};
}

Simulation::Simulation(MatchConfig config)
    : m_config(config),
      m_rng(config.deterministic ? Rng(config.seed) : Rng::fromEntropy()),
      m_elixirOne(config.startingElixir),
      m_elixirTwo(config.startingElixir) {
    m_board.setCombatRules(m_config.combat);

    // Player two (the AI) occupies the top of the arena, player one the bottom.
    m_board.addEntity(EntityFactory::create(EntityType::KING_TOWER, kCenterX, kPlayerTwoKingY, false, Lane::LEFT));
    m_board.addEntity(EntityFactory::create(EntityType::QUEEN_TOWER, kCenterX - 1 - kSideOffset, kPlayerTwoQueenY,
                                            false, Lane::LEFT));
    m_board.addEntity(
        EntityFactory::create(EntityType::QUEEN_TOWER, kCenterX + kSideOffset, kPlayerTwoQueenY, false, Lane::RIGHT));

    m_board.addEntity(EntityFactory::create(EntityType::KING_TOWER, kCenterX, kPlayerOneKingY, true, Lane::LEFT));
    m_board.addEntity(
        EntityFactory::create(EntityType::QUEEN_TOWER, kCenterX - 1 - kSideOffset, kPlayerOneQueenY, true, Lane::LEFT));
    m_board.addEntity(
        EntityFactory::create(EntityType::QUEEN_TOWER, kCenterX + kSideOffset, kPlayerOneQueenY, true, Lane::RIGHT));
}

void Simulation::step(float dt) {
    if (!isRunning()) {
        return;
    }

    m_elapsed += dt;
    regenerateElixir(dt);
    m_board.updateEntities(dt);
    m_board.handleCombat(m_rng);
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

bool Simulation::canAfford(EntityType type, bool isPlayerOne) const {
    return elixir(isPlayerOne) >= elixirCost(type);
}

bool Simulation::deploy(EntityType type, Lane lane, bool isPlayerOne) {
    if (!isRunning() || !canAfford(type, isPlayerOne)) {
        return false;
    }

    const SpawnPoint spawn = spawnPointFor(type, lane, isPlayerOne);
    m_board.addEntity(EntityFactory::create(type, spawn.x, spawn.y, isPlayerOne, lane));

    float& pool = isPlayerOne ? m_elixirOne : m_elixirTwo;
    pool -= elixirCost(type);
    return true;
}

int Simulation::towerHealth(bool isPlayerOne) const {
    int total = 0;
    for (const auto& entity : m_board.getEntities()) {
        if (isKingOrQueen(entity->getType()) && entity->getIsPlayer() == isPlayerOne) {
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
        if (entity->getType() != EntityType::KING_TOWER) {
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
