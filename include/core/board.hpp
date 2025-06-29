#pragma once
#include "entity/entity.hpp"
#include <vector>
#include <memory>

class Board {
public:
    void addEntity(std::shared_ptr<Entity> entity);
    void updateEntities();
    void handleCombat();
    std::vector<std::shared_ptr<Entity>>& getEntities();
    const std::vector<std::shared_ptr<Entity>>& getEntities() const;   
    const std::vector<std::shared_ptr<Entity>>& getAllEntities() const {
        return entities;
    }

private:
    std::vector<std::shared_ptr<Entity>> entities;
    bool isInRange(const Entity& attacker, const Entity& target, int range) const;
    int calculateDamage(const Entity& attacker, const Entity& target) const;
};
