#pragma once

#include <memory>
#include <string>

#include "clash_royale/core/types.hpp"
#include "clash_royale/sim/card.hpp"

namespace cr {

class Board;

/// A troop or building in play.
///
/// Entity used to be abstract, with a subclass per unit whose only job was to
/// assign five numbers in `calculateStats()`, plus four intermediate abstract
/// classes selecting a movement or targeting policy. All of that is now data on
/// a CardSpec, and this class is concrete: it reads its stats from the spec it
/// was built with.
///
/// It remains polymorphic. `update()`, `move()` and `findTarget()` are virtual,
/// so a card whose behavior genuinely cannot be described as data can supply a
/// subclass through `CardSpec::factory`.
class Entity {
public:
    /// `spec` must outlive this entity. A CardRegistry guarantees that for any
    /// spec it hands out.
    Entity(const CardSpec& spec, int x, int y, bool isPlayer, Lane lane);
    virtual ~Entity() = default;

    const CardSpec& spec() const { return *m_spec; }
    const std::string& cardId() const { return m_spec->id; }

    /// Advances this entity by `dt` seconds, stepping it once its move timer
    /// reaches the interval implied by its speed.
    virtual void update(const Board& board, float dt = kDefaultTimeStep);

    void takeDamage(int damage);
    bool isAlive() const;

    int getX() const { return m_x; }
    int getY() const { return m_y; }
    int getHealth() const { return m_health; }
    int getMaxHealth() const { return m_maxHealth; }
    bool getIsPlayer() const { return m_isPlayer; }
    Lane getLane() const { return m_homeLane; }

    bool isFlying() const { return m_spec->domain == MovementDomain::Air; }
    bool isBuilding() const { return m_spec->isBuilding; }
    int getAttackRange() const { return m_spec->attackRange; }
    int getDamage() const { return m_spec->damage; }

    /// Attacks per second. Combined with getDamage() this gives the unit's DPS.
    float getAttackSpeed() const { return m_spec->attackSpeed; }

    bool canAttackAir() const { return m_spec->targets.air; }

    /// Glyph for this entity: the card's symbol, lowercased for the opponent.
    char getSymbol() const;

    /// True when this entity's target filter admits `other`.
    ///
    /// Movement targeting and combat targeting both go through here, so the
    /// policy exists once. Previously Entity::findTarget and
    /// Board::handleCombat each carried their own copy, which is why the
    /// Golem's buildings-only preference had to be special-cased in both.
    bool canTarget(const Entity& other) const;

    /// True when this entity's attack cooldown has elapsed. A freshly created
    /// entity can attack immediately.
    bool canAttack() const { return m_attackCooldown <= 0.0f; }

    /// Advances the attack cooldown by `dt`. Called once per combat round for
    /// every living entity, whether or not it has a target.
    void tickAttackCooldown(float dt);

    /// Starts the cooldown after an attack lands.
    void registerAttack();

    /// The nearest enemy this entity is willing to attack, or nullptr.
    virtual std::shared_ptr<Entity> findTarget(const Board& board) const;

protected:
    /// Takes one movement step, following the card's MovementStyle.
    virtual void move(const Board& board);

    /// Moves one tile along whichever axis is further from (tx, ty).
    void stepAlongDominantAxis(int tx, int ty);

    /// Keeps this entity inside the playable interior of the arena.
    void clampToArena();

    void logWarning(const std::string& message) const;

    const CardSpec* m_spec;
    int m_x;
    int m_y;
    int m_health;
    int m_maxHealth;
    float m_moveTimer;
    float m_attackCooldown;
    bool m_isPlayer;
    Lane m_homeLane;

    /// Movement steps taken so far, for styles that vary between steps.
    int m_stepCount;
};

}  // namespace cr
