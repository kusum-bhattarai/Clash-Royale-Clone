#include "EntityFactory.hpp"
#include "Knight.hpp"
#include "Golem.hpp"
#include "Pekka.hpp"
#include "Goblins.hpp"
#include "Dragon.hpp"
#include "Wizard.hpp"
#include "Archers.hpp"
#include "Canon.hpp"
#include "KingTower.hpp"
#include "QueenTower.hpp"
#include <stdexcept> 

std::shared_ptr<Entity> EntityFactory::create(EntityType type, int x, int y, bool isPlayer, Lane lane) {
    switch (type) {
        case EntityType::KNIGHT:
            return std::make_shared<Knight>(type, x, y, isPlayer, 600, lane);
        case EntityType::GOLEM:
            return std::make_shared<Golem>(type, x, y, isPlayer, 500, lane);
        case EntityType::PEKKA:
            return std::make_shared<Pekka>(type, x, y, isPlayer, 600, lane);
        case EntityType::GOBLINS:
            return std::make_shared<Goblins>(type, x, y, isPlayer, 200, lane);
        case EntityType::DRAGON:
            return std::make_shared<Dragon>(type, x, y, isPlayer, 600, lane);
        case EntityType::WIZARD:
            return std::make_shared<Wizard>(type, x, y, isPlayer, 500, lane);
        case EntityType::ARCHERS:
            return std::make_shared<Archers>(type, x, y, isPlayer, 120, lane);
        case EntityType::CANON:
            return std::make_shared<Canon>(type, x, y, isPlayer, 500, lane);
        case EntityType::KING_TOWER:
            return std::make_shared<KingTower>(type, x, y, isPlayer, 4000, lane);
        case EntityType::QUEEN_TOWER:
            return std::make_shared<QueenTower>(type, x, y, isPlayer, 1500, lane);
        default:
            throw std::invalid_argument("Unknown entity type in factory");
    }
}