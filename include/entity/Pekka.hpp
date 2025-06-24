#pragma once
#include "entity/MovableEntity.hpp"

class Pekka : public MovableEntity {
public:
    Pekka(EntityType type, int x, int y, bool isPlayer, int health, Lane lane);
    void calculateStats() override;
    char getSymbol() const override { return m_isPlayer ? 'P' : 'p'; }
};