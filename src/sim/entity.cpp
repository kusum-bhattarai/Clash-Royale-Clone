#include "clash_royale/sim/entity.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <limits>

#include "clash_royale/core/arena.hpp"
#include "clash_royale/sim/board.hpp"
#include "clash_royale/sim/combat.hpp"

namespace cr {
namespace {

/// -1, 0 or +1 according to the sign of `value`.
int signum(int value) {
    return (value > 0) ? 1 : (value < 0 ? -1 : 0);
}

}  // namespace

Entity::Entity(const CardSpec& spec, const Arena& arena, int x, int y, bool isPlayer, Lane lane)
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
    if (!arena.inInterior(x, y)) {
        logWarning("Initial position out of bounds, clamping");
        arena.clampToInterior(m_x, m_y);
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

bool Entity::tryStep(const Arena& arena, int dx, int dy) {
    if (dx == 0 && dy == 0) {
        return false;
    }
    const int nx = m_x + dx;
    const int ny = m_y + dy;
    if (!arena.isPassable(nx, ny, m_spec->domain)) {
        return false;
    }
    m_x = nx;
    m_y = ny;
    return true;
}

void Entity::stepAlongDominantAxis(const Arena& arena, int tx, int ty) {
    const int dx = tx - m_x;
    const int dy = ty - m_y;

    const int stepX = signum(dx);
    const int stepY = signum(dy);

    // Ties resolve vertically, matching the original behavior.
    const bool horizontalFirst = std::abs(dx) > std::abs(dy);

    // Falling back to the other axis is what lets a unit walk the length of a
    // bridge: the step it wants may be sideways into water, and refusing it
    // without an alternative would leave the unit stuck on the bank.
    if (horizontalFirst) {
        if (!tryStep(arena, stepX, 0)) {
            tryStep(arena, 0, stepY);
        }
    } else {
        if (!tryStep(arena, 0, stepY)) {
            tryStep(arena, stepX, 0);
        }
    }
}

void Entity::waypointToward(const Arena& arena, const Entity& target, int& outX, int& outY) const {
    outX = target.getX();
    outY = target.getY();

    // Air units ignore terrain, and an arena without a river needs no routing.
    if (m_spec->domain == MovementDomain::Air || !arena.hasRiver()) {
        return;
    }

    const int mySide = arena.riverSide(m_y);
    const int targetSide = arena.riverSide(outY);

    // Already on the target's side, and not standing on the river itself.
    if (mySide == targetSide && mySide != 0) {
        return;
    }

    // Steer for this unit's crossing, aiming at the first row past the river on
    // the target's side so the unit commits to the bridge and walks off it
    // rather than stopping on top.
    outX = arena.bridgeColumnFor(m_homeLane, m_x);
    if (targetSide > 0) {
        outY = arena.riverRows().back() + 1;
    } else if (targetSide < 0) {
        outY = arena.riverRows().front() - 1;
    } else {
        outY = target.getY();  // the target is itself on the crossing
    }
}

void Entity::move(const Board& board) {
    if (m_spec->movement == MovementStyle::Stationary) {
        return;
    }

    const std::shared_ptr<Entity> target = findTarget(board);
    if (target == nullptr) {
        return;
    }

    const Arena& arena = board.arena();

    // Ranged units stop once the real target is in reach, regardless of
    // whether they are still routing toward a crossing.
    if (m_spec->movement == MovementStyle::HoldAtRange &&
        isWithinRange(*this, *target, m_spec->attackRange)) {
        return;
    }

    int goalX = 0;
    int goalY = 0;
    waypointToward(arena, *target, goalX, goalY);

    const int dx = goalX - m_x;
    const int dy = goalY - m_y;
    const int stepX = signum(dx);
    const int stepY = signum(dy);

    switch (m_spec->movement) {
        case MovementStyle::Stationary:
            return;

        case MovementStyle::AxisStep:
        case MovementStyle::HoldAtRange:
            stepAlongDominantAxis(arena, goalX, goalY);
            break;

        case MovementStyle::Diagonal:
            // Closes on both axes in the same step, then settles for one axis
            // if the diagonal tile is not enterable.
            if (!tryStep(arena, stepX, stepY)) {
                if (!tryStep(arena, stepX, 0)) {
                    tryStep(arena, 0, stepY);
                }
            }
            break;

        case MovementStyle::Zigzag:
            // Alternates which axis it closes on, rather than exhausting one
            // before starting the other.
            if ((m_stepCount % 2) == 0) {
                if (!tryStep(arena, stepX, 0)) {
                    tryStep(arena, 0, stepY);
                }
            } else {
                if (!tryStep(arena, 0, stepY)) {
                    tryStep(arena, stepX, 0);
                }
            }
            break;
    }
}

void Entity::logWarning(const std::string& message) const {
    std::cerr << "Warning [Entity " << getSymbol() << " at (" << m_x << "," << m_y << ")]: " << message << std::endl;
}

}  // namespace cr
