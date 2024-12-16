#pragma once
#include "entity.hpp"
#include <vector>
#include <memory>

class Board {
public:
    void addEntity(std::shared_ptr<Entity> entity);
    void updateEntities();
    void handleCombat();
    const std::vector<std::shared_ptr<Entity>>& getEntities() const;
    
    static void setCurrentBoard(Board* board) { currentBoard = board; }
    static const Board* getCurrentBoard() {
        return currentBoard;
    }
    
    const std::vector<std::shared_ptr<Entity>>& getAllEntities() const {
        return entities;
    }

private:
    std::vector<std::shared_ptr<Entity>> entities;
    static Board* currentBoard;
    static std::vector<std::shared_ptr<Entity>> emptyEntities;
    bool isInRange(const Entity& attacker, const Entity& target, int range) const;
    int calculateDamage(const Entity& attacker, const Entity& target) const;
};
