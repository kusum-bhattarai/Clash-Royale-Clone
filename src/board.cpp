#include "board.hpp"
#include <cmath>
#include <algorithm>
#include <climits>
#include <limits>
using namespace std;

void Board::addEntity(std::shared_ptr<Entity> entity) {
    entities.push_back(entity);
}

void Board::updateEntities() {
    for (auto& entity : entities) {
        if (entity->isAlive()) {
            entity->update(*this);
        }
    }
    
    // Remove dead entities except towers
    entities.erase(
        std::remove_if(entities.begin(), entities.end(),
                      [](const std::shared_ptr<Entity>& e) {
                          return !e->isAlive() && 
                                 e->getType() != EntityType::KING_TOWER &&
                                 e->getType() != EntityType::QUEEN_TOWER;
                      }),
        entities.end());
}

bool Board::isInRange(const Entity& attacker, const Entity& target, int range) const {
    int dx = attacker.getX() - target.getX();
    int dy = attacker.getY() - target.getY();
    return (dx * dx + dy * dy) <= range * range;
}

int Board::calculateDamage(const Entity& attacker, const Entity& target) const {
    int baseDamage = attacker.getDamage();
    float multiplier = 1.0f;

    // Apply damage modifiers based on target type
    switch (target.getType()) {
        case EntityType::KING_TOWER:
        case EntityType::QUEEN_TOWER:
            if (attacker.getType() == EntityType::GOLEM) {
                multiplier = 1.5f;  // Golem deals extra damage to buildings
            }
            break;
            
        case EntityType::CANON:
            if (attacker.getType() == EntityType::PEKKA) {
                multiplier = 1.3f;  // PEKKA deals extra damage to buildings
            }
        default: 
            break;
    }

    // Armor type modifications
    switch (attacker.getType()) {
        case EntityType::WIZARD:
            if (target.isFlying()) {
                multiplier *= 1.2f;  // Wizard deals extra damage to air units
            }
            break;
            
        case EntityType::PEKKA:
            multiplier *= 1.2f;  // PEKKA deals extra damage due to heavy armor penetration
            break;
            
        case EntityType::GOBLINS:
            if (target.getType() == EntityType::KNIGHT || 
                target.getType() == EntityType::WIZARD ||
                target.getType() == EntityType::ARCHERS) {
                multiplier *= 1.1f;  // Goblins deal extra damage to light armor
            }
        default:
            break;
    }

    // Critical hit chance (5%)
    if (rand() % 100 < 5) {
        multiplier *= 1.5f;
    }

    // Calculate final damage
    int finalDamage = static_cast<int>(baseDamage * multiplier);
    
    // Apply damage reduction based on armor type
    switch (target.getType()) {
        case EntityType::KNIGHT:
            finalDamage = static_cast<int>(finalDamage * 0.8f);  // Medium armor
            break;
        case EntityType::GOLEM:
        case EntityType::PEKKA:
            finalDamage = static_cast<int>(finalDamage * 0.6f);  // Heavy armor
            break;
        case EntityType::GOBLINS:
        case EntityType::WIZARD:
        case EntityType::ARCHERS:
            finalDamage = static_cast<int>(finalDamage * 1.0f);  // Light armor
            break;
        default:
            break;
    }
    
    // Critical hit chance (5%)
    if (rand() % 100 < 5) {
        finalDamage = static_cast<int>(finalDamage * 1.5f);
    }
    
    // Minimum damage is 1
    return std::max(1, finalDamage);
}

void Board::handleCombat() {
    for (auto& attacker : entities) {
        if (!attacker->isAlive()) continue;
        
        std::shared_ptr<Entity> bestTarget = nullptr;
        int minDistance = INT_MAX;
        
        for (auto& target : entities) {
            if (!target->isAlive() || 
                target->getIsPlayer() == attacker->getIsPlayer()) continue;
            
            // Check if attacker can target flying units
            if (target->isFlying() && !attacker->canAttackAir()) continue;
            
            // Use the entity's attack range
            int range = attacker->getAttackRange();
            
            int distance = std::abs(attacker->getX() - target->getX()) +
                         std::abs(attacker->getY() - target->getY());
                         
            if (isInRange(*attacker, *target, range) && distance < minDistance) {
                // Prioritize buildings for certain units
                if (attacker->getType() == EntityType::GOLEM) {
                    if (target->getType() == EntityType::KING_TOWER ||
                        target->getType() == EntityType::QUEEN_TOWER ||
                        target->getType() == EntityType::CANON) {
                        bestTarget = target;
                        minDistance = distance;
                    }
                } else {
                    bestTarget = target;
                    minDistance = distance;
                }
            }
        }
        
        if (bestTarget) {
            int damage = calculateDamage(*attacker, *bestTarget);
            bestTarget->takeDamage(damage);
        }
    }
}

const std::vector<std::shared_ptr<Entity>>& Board::getEntities() const {
    return entities;
}
