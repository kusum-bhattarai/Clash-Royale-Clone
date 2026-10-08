#include "clash_royale/sim/entity.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <limits>

#include "clash_royale/core/arena.hpp"
#include "clash_royale/path/navigator.hpp"
#include "clash_royale/sim/board.hpp"
#include "clash_royale/sim/combat.hpp"

namespace cr {
namespace {

/// Steps between forced recomputations of a route.
constexpr int kRepathSteps = 6;

/// How far a target may drift from where a route was aimed before the route is
/// considered stale.
constexpr int kGoalDriftTolerance = 2;

/// Consecutive fully-blocked steps after which a unit asks for a route that
/// avoids other units rather than only terrain.
constexpr int kStuckSteps = 3;

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
      m_stepCount(0),
      m_routeIndex(0),
      m_routeGoal{x, y},
      // Stagger repathing by spawn position so a wave of units deployed
      // together does not recompute on the same step.
      m_repathIn((x + y) % kRepathSteps),
      m_blockedSteps(0),
      m_avoidCongestion(false) {
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

bool Entity::tryStep(const Board& board, int dx, int dy) {
    if (dx == 0 && dy == 0) {
        return false;
    }

    const int nx = m_x + dx;
    const int ny = m_y + dy;

    if (!board.arena().isPassable(nx, ny, m_spec->domain)) {
        return false;
    }
    // Units of the same domain block each other; a flier and a ground troop do
    // not. Buildings occupy their tile too, so troops walk around them.
    if (board.occupancyAt(nx, ny, m_spec->domain) > 0) {
        return false;
    }

    m_x = nx;
    m_y = ny;
    return true;
}

bool Entity::stepToward(const Board& board, Point next) {
    const int dx = signum(next.x - m_x);
    const int dy = signum(next.y - m_y);

    // Candidate moves in preference order. A diagonal route step is split into
    // two moves for the axis gaits, so a ground unit still covers one tile per
    // step and its speed means what it did before pathfinding.
    int candidates[3][2] = {{0, 0}, {0, 0}, {0, 0}};

    switch (m_spec->movement) {
        case MovementStyle::Stationary:
            return false;

        case MovementStyle::Diagonal:
            candidates[0][0] = dx;
            candidates[0][1] = dy;
            candidates[1][0] = dx;
            candidates[2][1] = dy;
            break;

        case MovementStyle::AxisStep: {
            // Lead on whichever axis has further to go overall, so a unit
            // crossing the arena keeps a steady heading instead of staircasing.
            const bool horizontalFirst = std::abs(m_routeGoal.x - m_x) > std::abs(m_routeGoal.y - m_y);
            if (horizontalFirst) {
                candidates[0][0] = dx;
                candidates[1][1] = dy;
            } else {
                candidates[0][1] = dy;
                candidates[1][0] = dx;
            }
            candidates[2][0] = dx;
            candidates[2][1] = dy;
            break;
        }

        case MovementStyle::Zigzag:
            // Alternates axes between steps regardless of heading.
            if ((m_stepCount % 2) == 0) {
                candidates[0][0] = dx;
                candidates[1][1] = dy;
            } else {
                candidates[0][1] = dy;
                candidates[1][0] = dx;
            }
            candidates[2][0] = dx;
            candidates[2][1] = dy;
            break;
    }

    for (const auto& candidate : candidates) {
        if (tryStep(board, candidate[0], candidate[1])) {
            return true;
        }
    }
    return false;
}

void Entity::ensureRoute(const Board& board, Point goal) {
    const bool exhausted = m_routeIndex >= m_route.size();
    const int drift = std::abs(goal.x - m_routeGoal.x) + std::abs(goal.y - m_routeGoal.y);

    if (!exhausted && drift <= kGoalDriftTolerance && m_repathIn > 0 && !m_avoidCongestion) {
        return;
    }

    const BoardObstacles obstacles = board.obstacles();

    PathRequest request;
    request.from = Point{m_x, m_y};
    request.to = goal;
    request.domain = m_spec->domain;
    // Only a unit that local avoidance has failed pays for a route that
    // accounts for other units; everyone else shares the terrain-only field.
    request.obstacles = m_avoidCongestion ? &obstacles : nullptr;

    const std::optional<Path> route = board.navigator().route(board.arena(), request);
    m_route = route.value_or(Path{});
    m_routeIndex = 0;
    m_routeGoal = goal;
    m_repathIn = kRepathSteps;
    m_avoidCongestion = false;
}

void Entity::move(const Board& board) {
    if (m_spec->movement == MovementStyle::Stationary) {
        return;
    }

    const std::shared_ptr<Entity> target = findTarget(board);
    if (target == nullptr) {
        m_route.clear();
        m_routeIndex = 0;
        return;
    }

    // Every unit stops advancing once its target is in reach. This used to be
    // the HoldAtRange gait, which only ranged units had; melee units walked
    // onto the tile their target was standing on.
    if (isWithinRange(*this, *target, m_spec->attackRange)) {
        m_route.clear();
        m_routeIndex = 0;
        return;
    }

    --m_repathIn;
    ensureRoute(board, Point{target->getX(), target->getY()});

    if (m_routeIndex >= m_route.size()) {
        return;  // nowhere to go
    }

    const Point next = m_route[m_routeIndex];
    if (stepToward(board, next)) {
        m_blockedSteps = 0;
        if (Point{m_x, m_y} == next) {
            ++m_routeIndex;
        }
    } else if (++m_blockedSteps >= kStuckSteps) {
        // Hemmed in by other units. Ask for a route that routes around them.
        m_blockedSteps = 0;
        m_avoidCongestion = true;
    }
}

void Entity::logWarning(const std::string& message) const {
    std::cerr << "Warning [Entity " << getSymbol() << " at (" << m_x << "," << m_y << ")]: " << message << std::endl;
}

}  // namespace cr
