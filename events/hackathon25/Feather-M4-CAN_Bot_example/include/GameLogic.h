#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include <Arduino.h>
#include <vector>
#include "Hackathon25.h"

// Position structure
struct Position {
    uint8_t x, y;
    float cost, g, h;
    Position* parent;

    Position(uint8_t x, uint8_t y, float g, float h, Position* parent = nullptr)
        : x(x), y(y), cost(g + h), g(g), h(h), parent(parent) {}

    bool operator>(const Position& other) const {
        return cost > other.cost;
    }
};

// Game logic functions
void process_GameState(uint8_t* data);
void process_Die(uint8_t* data);
void process_GameFinish(uint8_t* data);
void process_Error(uint8_t* data);

#endif