#include "clash_royale/sim/card_registry.hpp"

#include <stdexcept>
#include <string>

#include "clash_royale/sim/default_cards.hpp"
#include "clash_royale/sim/entity.hpp"

namespace cr {
namespace {

/// Incoming-damage scaling for each armor class. These were a switch statement
/// in the damage calculation; they are now the default for a spec's armor and
/// can still be overridden per card.
float armorScaling(ArmorClass armor) {
    switch (armor) {
        case ArmorClass::Light:
            return 1.0f;
        case ArmorClass::Medium:
            return 0.8f;
        case ArmorClass::Heavy:
            return 0.6f;
        case ArmorClass::Building:
            return 1.0f;
    }
    return 1.0f;
}

CardSpec troop(std::string_view id, std::string_view name, char symbol, int health, int damage, int range,
               float moveSpeed, float hitSeconds, float cost, ArmorClass armor) {
    CardSpec spec;
    spec.id = std::string(id);
    spec.displayName = std::string(name);
    spec.symbol = symbol;
    spec.health = health;
    spec.damage = damage;
    spec.attackRange = range;
    spec.moveSpeed = moveSpeed;
    spec.attackSpeed = 1.0f / hitSeconds;
    spec.elixirCost = cost;
    spec.armor = armor;
    spec.incomingDamageMultiplier = armorScaling(armor);
    return spec;
}

CardSpec building(std::string_view id, std::string_view name, char symbol, int health, int damage, int range,
                  float hitSeconds, float cost) {
    CardSpec spec = troop(id, name, symbol, health, damage, range, 0.0f, hitSeconds, cost, ArmorClass::Building);
    spec.isBuilding = true;
    spec.movement = MovementStyle::Stationary;
    return spec;
}

}  // namespace

const CardSpec& CardRegistry::define(CardSpec spec) {
    const std::string id = spec.id;
    if (id.empty()) {
        throw std::invalid_argument("CardSpec::id must not be empty");
    }

    if (auto it = m_index.find(id); it != m_index.end()) {
        *it->second = std::move(spec);  // overwrite in place, keeping the address stable
        rebuildViews();
        return *it->second;
    }

    m_storage.push_back(std::move(spec));
    CardSpec* stored = &m_storage.back();
    m_index.emplace(id, stored);
    rebuildViews();
    return *stored;
}

void CardRegistry::rebuildViews() {
    m_all.clear();
    m_deployable.clear();
    for (const CardSpec& spec : m_storage) {
        m_all.push_back(&spec);
        if (spec.deployable) {
            m_deployable.push_back(&spec);
        }
    }
}

const CardSpec* CardRegistry::find(std::string_view id) const {
    const auto it = m_index.find(std::string(id));
    return (it == m_index.end()) ? nullptr : it->second;
}

const CardSpec& CardRegistry::get(std::string_view id) const {
    if (const CardSpec* spec = find(id)) {
        return *spec;
    }
    throw std::out_of_range("unknown card id: " + std::string(id));
}

CardRegistry CardRegistry::withDefaultCards() {
    CardRegistry registry;

    //                     id                 name           sym  hp  dmg rng spd  hit  cost armor
    CardSpec knight = troop(cards::Knight, "Knight", 'K', 600, 50, 1, 1.00f, 1.2f, 4.0f, ArmorClass::Medium);
    knight.targets = TargetFilter{/*ground=*/true, /*air=*/false};
    registry.define(knight);

    CardSpec golem = troop(cards::Golem, "Golem", 'G', 500, 50, 1, 0.50f, 2.5f, 5.0f, ArmorClass::Heavy);
    // Replaces the TowerPrioritizingEntity subclass: the Golem ignores troops.
    golem.targets = TargetFilter{true, false, /*buildingsOnly=*/true};
    golem.damageModifiers = {
        DamageModifier{1.5f, {}, {}, {std::string(cards::KingTower), std::string(cards::QueenTower)}, {}},
    };
    registry.define(golem);

    CardSpec pekka = troop(cards::Pekka, "P.E.K.K.A", 'P', 600, 70, 1, 0.75f, 1.8f, 4.0f, ArmorClass::Heavy);
    pekka.targets = TargetFilter{true, false};
    pekka.damageModifiers = {
        DamageModifier{1.2f, {}, {}, {}, {}},                                  // heavy armor penetration
        DamageModifier{1.3f, {}, {}, {std::string(cards::Canon)}, {}},         // hits buildings harder
    };
    registry.define(pekka);

    CardSpec goblins = troop(cards::Goblins, "Goblins", 'B', 200, 20, 1, 1.20f, 1.1f, 3.0f, ArmorClass::Light);
    goblins.movement = MovementStyle::Zigzag;
    goblins.targets = TargetFilter{true, false};
    goblins.damageModifiers = {
        DamageModifier{1.1f,
                       {},
                       {},
                       {std::string(cards::Knight), std::string(cards::Wizard), std::string(cards::Archers)},
                       {}},
    };
    registry.define(goblins);

    CardSpec dragon = troop(cards::Dragon, "Baby Dragon", 'D', 600, 50, 3, 1.50f, 1.8f, 5.0f, ArmorClass::Light);
    dragon.domain = MovementDomain::Air;
    dragon.movement = MovementStyle::Diagonal;
    dragon.targets = TargetFilter{true, /*air=*/true};
    registry.define(dragon);

    CardSpec wizard = troop(cards::Wizard, "Wizard", 'W', 500, 65, 5, 1.00f, 1.4f, 4.0f, ArmorClass::Light);
    wizard.movement = MovementStyle::HoldAtRange;
    wizard.targets = TargetFilter{true, true};
    wizard.damageModifiers = {
        DamageModifier{1.2f, {}, {MovementDomain::Air}, {}, {}},  // splash is effective against air
    };
    registry.define(wizard);

    CardSpec archers = troop(cards::Archers, "Archers", 'A', 120, 20, 7, 1.20f, 1.2f, 2.0f, ArmorClass::Light);
    archers.movement = MovementStyle::HoldAtRange;
    archers.targets = TargetFilter{true, true};
    registry.define(archers);

    CardSpec canon = building(cards::Canon, "Canon", 'C', 500, 40, 5, 0.8f, 3.0f);
    canon.targets = TargetFilter{true, /*air=*/false};
    canon.spawnSetback = 2;
    registry.define(canon);

    CardSpec queen = building(cards::QueenTower, "Queen Tower", 'Q', 1500, 50, 5, 0.8f, 0.0f);
    queen.targets = TargetFilter{true, true};
    queen.towerRole = TowerRole::Queen;
    queen.deployable = false;
    registry.define(queen);

    CardSpec king = building(cards::KingTower, "King Tower", 'T', 4000, 75, 7, 1.0f, 0.0f);
    king.targets = TargetFilter{true, true};
    king.towerRole = TowerRole::King;
    king.deployable = false;
    registry.define(king);

    return registry;
}

std::shared_ptr<Entity> createEntity(const CardSpec& spec, int x, int y, bool isPlayer, Lane lane) {
    if (spec.factory) {
        return spec.factory(spec, x, y, isPlayer, lane);
    }
    return std::make_shared<Entity>(spec, x, y, isPlayer, lane);
}

}  // namespace cr
