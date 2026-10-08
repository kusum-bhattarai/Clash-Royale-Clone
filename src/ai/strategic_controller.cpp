#include "clash_royale/ai/strategic_controller.hpp"

#include <algorithm>
#include <cmath>

#include "clash_royale/core/arena.hpp"
#include "clash_royale/sim/card.hpp"
#include "clash_royale/sim/combat.hpp"
#include "clash_royale/sim/entity.hpp"
#include "clash_royale/sim/simulation.hpp"

namespace cr {
namespace {

/// Damage per second a card deals, ignoring matchups.
float damagePerSecond(const CardSpec& spec) {
    return static_cast<float>(spec.damage) * spec.attackSpeed;
}

/// Health scaled by armor, so a heavily armored card reads as the larger
/// health pool it effectively has.
float effectiveHealth(const CardSpec& spec) {
    const float incoming = (spec.incomingDamageMultiplier > 0.01f) ? spec.incomingDamageMultiplier : 1.0f;
    return static_cast<float>(spec.health) / incoming;
}

/// Whether `defender` is able to attack `threat` at all.
///
/// Read off the target filter rather than from any knowledge of which cards
/// exist, so a custom card is classified correctly.
bool canEngage(const CardSpec& defender, const Entity& threat) {
    if (threat.isFlying() ? !defender.targets.air : !defender.targets.ground) {
        return false;
    }
    // A card that only attacks buildings walks straight past a push.
    return !(defender.targets.buildingsOnly && !threat.isBuilding());
}

/// A unit's contribution to a fight: its damage output, with a floor so that
/// even a harmless body counts for something as a blocker.
float unitPower(const CardSpec& spec) {
    return damagePerSecond(spec) + 5.0f;
}

/// How dangerous a unit is.
float threatWeight(const Entity& entity) {
    return unitPower(entity.spec());
}

float squaredDistance(const Entity& a, const Entity& b) {
    const float dx = static_cast<float>(a.getX() - b.getX());
    const float dy = static_cast<float>(a.getY() - b.getY());
    return dx * dx + dy * dy;
}

}  // namespace

std::vector<const Entity*> StrategicAiController::findThreats(const Simulation& sim, bool isPlayerOne) const {
    std::vector<const Entity*> threats;
    const float radiusSq = m_tuning.threatRadius * m_tuning.threatRadius;

    for (const auto& candidate : sim.board().getEntities()) {
        if (!candidate->isAlive() || candidate->getIsPlayer() == isPlayerOne) {
            continue;
        }
        if (candidate->spec().towerRole != TowerRole::None) {
            continue;  // their towers are not a threat to answer
        }

        // Anything closing on one of our towers counts, regardless of which
        // half of the arena it has reached.
        for (const auto& own : sim.board().getEntities()) {
            if (own->getIsPlayer() != isPlayerOne || own->spec().towerRole == TowerRole::None ||
                !own->isAlive()) {
                continue;
            }
            if (squaredDistance(*candidate, *own) <= radiusSq) {
                threats.push_back(candidate.get());
                break;
            }
        }
    }
    return threats;
}

float StrategicAiController::committedDefence(const Simulation& sim, bool isPlayerOne,
                                              const std::vector<const Entity*>& threats) const {
    const float coverSq = m_tuning.coverRadius * m_tuning.coverRadius;
    float total = 0.0f;

    for (const auto& own : sim.board().getEntities()) {
        if (!own->isAlive() || own->getIsPlayer() != isPlayerOne) {
            continue;
        }

        for (const Entity* threat : threats) {
            if (!canEngage(own->spec(), *threat)) {
                continue;
            }

            // Towers defend from where they stand, so what matters is whether
            // the threat is inside their reach rather than how near they are.
            const float reach = (own->spec().towerRole != TowerRole::None)
                                    ? static_cast<float>(own->getAttackRange() + 2)
                                    : m_tuning.coverRadius;
            const float reachSq = (own->spec().towerRole != TowerRole::None) ? reach * reach : coverSq;

            if (squaredDistance(*own, *threat) <= reachSq) {
                total += unitPower(own->spec());
                break;  // counted once, however many threats it covers
            }
        }
    }
    return total;
}

const CardSpec* StrategicAiController::chooseDefender(const Simulation& sim, bool isPlayerOne,
                                                      const std::vector<const Entity*>& threats) const {
    const CardSpec* best = nullptr;
    float bestScore = 0.0f;

    float totalWeight = 0.0f;
    for (const Entity* threat : threats) {
        totalWeight += threatWeight(*threat);
    }
    if (totalWeight <= 0.0f) {
        return nullptr;
    }

    for (const CardSpec* card : sim.cards().deployable()) {
        if (!sim.canAfford(card->id, isPlayerOne)) {
            continue;
        }

        // How much of the incoming push this card can actually fight, weighted
        // by how dangerous each part of it is and by any damage bonus the card
        // enjoys against that particular unit.
        float coverage = 0.0f;
        for (const Entity* threat : threats) {
            if (canEngage(*card, *threat)) {
                coverage += threatWeight(*threat) * matchupMultiplier(*card, threat->spec());
            }
        }
        if (coverage <= 0.0f) {
            continue;  // cannot touch any of it; never worth deploying
        }

        const float quality =
            damagePerSecond(*card) * m_tuning.dpsWeight + effectiveHealth(*card) * m_tuning.toughnessWeight;
        const float cost = std::max(0.5f, card->elixirCost);
        const float score = quality * (coverage / totalWeight) / cost;

        if (score > bestScore) {
            bestScore = score;
            best = card;
        }
    }
    return best;
}

const CardSpec* StrategicAiController::chooseAttacker(const Simulation& sim, bool isPlayerOne) const {
    const CardSpec* best = nullptr;
    float bestScore = 0.0f;

    for (const CardSpec* card : sim.cards().deployable()) {
        if (!sim.canAfford(card->id, isPlayerOne)) {
            continue;
        }

        float score = (damagePerSecond(*card) + effectiveHealth(*card) * 0.05f) / std::max(0.5f, card->elixirCost);

        // A card that ignores troops walks past defenders and reaches the
        // tower, which is what an attack is for.
        if (card->targets.buildingsOnly) {
            score *= m_tuning.towerFocusBonus;
        }
        // Something that cannot move is a defensive investment, not a push.
        if (card->movement == MovementStyle::Stationary) {
            score *= m_tuning.immobilePenalty;
        }

        if (score > bestScore) {
            bestScore = score;
            best = card;
        }
    }
    return best;
}

Lane StrategicAiController::threatenedLane(const Arena& arena, const std::vector<const Entity*>& threats) const {
    const Entity* heaviest = nullptr;
    float heaviestWeight = -1.0f;
    for (const Entity* threat : threats) {
        const float weight = threatWeight(*threat);
        if (weight > heaviestWeight) {
            heaviestWeight = weight;
            heaviest = threat;
        }
    }
    if (heaviest == nullptr) {
        return Lane::LEFT;
    }

    // Answer in the lane the threat is actually in, which is the lane whose
    // crossing it is nearest.
    const auto& bridges = arena.bridgeColumns();
    if (bridges.size() < 2) {
        return (heaviest->getX() * 2 < arena.width()) ? Lane::LEFT : Lane::RIGHT;
    }
    const int toLeft = std::abs(heaviest->getX() - bridges.front());
    const int toRight = std::abs(heaviest->getX() - bridges.back());
    return (toLeft <= toRight) ? Lane::LEFT : Lane::RIGHT;
}

Lane StrategicAiController::weakestEnemyLane(const Simulation& sim, bool isPlayerOne) const {
    const Arena& arena = sim.arena();
    const auto& bridges = arena.bridgeColumns();
    const int leftColumn = bridges.empty() ? arena.width() / 4 : bridges.front();
    const int rightColumn = bridges.empty() ? arena.width() * 3 / 4 : bridges.back();

    int leftHealth = 0;
    int rightHealth = 0;
    for (const auto& entity : sim.board().getEntities()) {
        if (entity->getIsPlayer() == isPlayerOne || entity->spec().towerRole != TowerRole::Queen) {
            continue;
        }
        const int toLeft = std::abs(entity->getX() - leftColumn);
        const int toRight = std::abs(entity->getX() - rightColumn);
        if (toLeft <= toRight) {
            leftHealth += entity->getHealth();
        } else {
            rightHealth += entity->getHealth();
        }
    }

    // Concentrate on whichever side is already weaker. Ties go left, which is
    // arbitrary but stable, and stability matters more than the choice: an AI
    // that alternated would split its own pushes.
    return (leftHealth <= rightHealth) ? Lane::LEFT : Lane::RIGHT;
}

void StrategicAiController::update(Simulation& sim, bool isPlayerOne, float dt) {
    if (!sim.isRunning()) {
        return;
    }
    m_sinceLastPush += dt;

    m_sinceLastDefence += dt;

    // Defence comes first and ignores the push reserve: a threat reaching a
    // tower costs more than the elixir spent stopping it. But only when the
    // threat is not already covered -- answering a push on every step was how
    // an earlier version spent its whole economy and never attacked at all,
    // holding an average of 1.2 elixir across a match.
    const std::vector<const Entity*> threats = findThreats(sim, isPlayerOne);
    if (!threats.empty() && m_sinceLastDefence >= m_tuning.defenceInterval) {
        float threatPower = 0.0f;
        for (const Entity* threat : threats) {
            threatPower += threatWeight(*threat);
        }

        if (committedDefence(sim, isPlayerOne, threats) < threatPower * m_tuning.defenceSufficiency) {
            if (const CardSpec* defender = chooseDefender(sim, isPlayerOne, threats)) {
                if (sim.deploy(defender->id, threatenedLane(sim.arena(), threats), isPlayerOne)) {
                    ++m_telemetry.defensiveDeploys;
                    m_sinceLastDefence = 0.0f;
                    return;
                }
            }
        }
    }

    // Otherwise bank elixir until there is enough for a push worth making, and
    // space pushes out so units arrive together.
    if (sim.elixir(isPlayerOne) >= m_tuning.pushElixir && m_sinceLastPush >= m_tuning.pushInterval) {
        if (const CardSpec* attacker = chooseAttacker(sim, isPlayerOne)) {
            if (sim.deploy(attacker->id, weakestEnemyLane(sim, isPlayerOne), isPlayerOne)) {
                ++m_telemetry.offensiveDeploys;
                m_sinceLastPush = 0.0f;
                return;
            }
        }
    }

    ++m_telemetry.stepsHeld;
}

}  // namespace cr
