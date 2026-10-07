#include "clash_royale/sim/entity.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <limits>

#include "clash_royale/sim/board.hpp"
#include "clash_royale/sim/combat.hpp"

namespace cr {
namespace {

int signum(int value) {
    return (value > 0) ? 1 : -1;
}

}  // namespace

Entity::Entity(const CardSpec& spec, int x, int y, bool isPlayer, Lane lane)
    : m_spec(&spec),
      m_x(x),
      m_y(y),
      m_health(spec.health),
      m_maxHealth(spec.health),
      m_moveTimer(0.0f),
      m_attackCooldown(0.0f),
      m_isPlayer(isPlayer),
      m_homeLane(lane),
      m_stepCount(0) {
    if (x < 1 || x >= kArenaWidth - 1 || y < 1 || y >= kArenaHeight - 1) {
        logWarning("Initial position out of bounds, clamping");
        clampToArena();
    }
    if (m_maxHealth <= 0) {
        logWarning("Card health invalid, setting to 1");
        m_health = 1;
        // m_maxHealth was previously left at the invalid value while m_health
        // was corrected, which let a zero reach the renderer's health-bar
        // division once bars started reading real maximums.
        m_maxHealth = 1;
    }
}

void Entity::update(const Board& board, float dt) {
    // Stationary cards skip the timer entirely.
    if (m_spec->moveSpeed <= 0.0f) {
        return;
    }

    m_moveTimer += dt;
    if (m_moveTimer >= 1.0f / m_spec->moveSpeed) {
        m_moveTimer = 0.0f;
        move(board);
        ++m_stepCount;
    }
}

void Entity::takeDamage(int damage) {
    if (damage < 0) {
        logWarning("Negative damage received");
        return;
    }
    m_health -= damage;
    if (m_health < 0) {
        m_health = 0;
    }
}

bool Entity::isAlive() const {
    return m_health > 0;
}

char Entity::getSymbol() const {
    const char symbol = m_spec->symbol;
    return m_isPlayer ? symbol : static_cast<char>(std::tolower(static_cast<unsigned char>(symbol)));
}

bool Entity::canTarget(const Entity& other) const {
    const TargetFilter& filter = m_spec->targets;

    if (other.isFlying()) {
        if (!filter.air) {
            return false;
        }
    } else if (!filter.ground) {
        return false;
    }

    if (filter.buildingsOnly && !other.isBuilding()) {
        return false;
    }
    return true;
}

void Entity::tickAttackCooldown(float dt) {
    if (m_attackCooldown > 0.0f) {
        m_attackCooldown -= dt;
    }
}

void Entity::registerAttack() {
    m_attackCooldown = (m_spec->attackSpeed > 0.0f) ? 1.0f / m_spec->attackSpeed : 0.0f;
}

std::shared_ptr<Entity> Entity::findTarget(const Board& board) const {
    std::shared_ptr<Entity> best = nullptr;
    int closest = std::numeric_limits<int>::max();

    for (const auto& candidate : board.getAllEntities()) {
        if (!candidate->isAlive() || candidate->getIsPlayer() == m_isPlayer) {
            continue;
        }
        if (!canTarget(*candidate)) {
            continue;
        }

        const int dx = candidate->getX() - m_x;
        const int dy = candidate->getY() - m_y;
        const int distanceSq = dx * dx + dy * dy;
        if (distanceSq < closest) {
            closest = distanceSq;
            best = candidate;
        }
    }
    return best;
}

void Entity::stepAlongDominantAxis(int tx, int ty) {
    const int dx = tx - m_x;
    const int dy = ty - m_y;

    // Ties resolve vertically, matching the original behavior.
    if (std::abs(dx) > std::abs(dy)) {
        m_x += signum(dx);
    } else if (dy != 0) {
        m_y += signum(dy);
    }
}

void Entity::clampToArena() {
    m_x = std::max(1, std::min(m_x, kArenaWidth - 2));
    m_y = std::max(1, std::min(m_y, kArenaHeight - 2));
}

void Entity::move(const Board& board) {
    if (m_spec->movement == MovementStyle::Stationary) {
        return;
    }

    const std::shared_ptr<Entity> target = findTarget(board);
    if (target == nullptr) {
        return;
    }

    const int tx = target->getX();
    const int ty = target->getY();
    const int dx = tx - m_x;
    const int dy = ty - m_y;

    switch (m_spec->movement) {
        case MovementStyle::Stationary:
            return;

        case MovementStyle::AxisStep:
            stepAlongDominantAxis(tx, ty);
            break;

        case MovementStyle::Diagonal:
            // Closes on both axes in the same step.
            if (dx != 0) {
                m_x += signum(dx);
            }
            if (dy != 0) {
                m_y += signum(dy);
            }
            break;

        case MovementStyle::Zigzag:
            // Alternates which axis it closes on, rather than exhausting one
            // before starting the other.
            if ((m_stepCount % 2) == 0) {
                if (dx != 0) {
                    m_x += signum(dx);
                } else if (dy != 0) {
                    m_y += signum(dy);
                }
            } else {
                if (dy != 0) {
                    m_y += signum(dy);
                } else if (dx != 0) {
                    m_x += signum(dx);
                }
            }
            break;

        case MovementStyle::HoldAtRange:
            // Stops advancing once the target is within reach.
            if (isWithinRange(*this, *target, m_spec->attackRange)) {
                return;
            }
            stepAlongDominantAxis(tx, ty);
            break;
    }

    clampToArena();
}

void Entity::logWarning(const std::string& message) const {
    std::cerr << "Warning [Entity " << getSymbol() << " at (" << m_x << "," << m_y << ")]: " << message << std::endl;
}

}  // namespace cr
