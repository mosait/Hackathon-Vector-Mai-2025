// Feather-m4-can_bot_example/include/GameLogic.h
/**
 * @file GameLogic.h
 * @brief Game strategy and decision-making logic for Tron game
 *
 * Defines:
 * - Position structure for path planning
 * - Algorithm helper functions
 * - Game message processing functions
 * - Grid and player trace storage
 */

#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include <Arduino.h>
#include <vector>
#include "Hackathon25.h"

/**
 * Position structure used for path planning and evaluation
 * Implements comparison operators for use in priority queues
 */
struct Position
{
    uint8_t x, y;     // Coordinates
    float cost, g, h; // Path costs (g=known cost, h=heuristic estimate)
    Position *parent; // Parent position for path reconstruction

    /**
     * Constructor initializing position and path costs
     */
    Position(uint8_t x = 0, uint8_t y = 0, float g = 0, float h = 0, Position *parent = nullptr)
        : x(x), y(y), cost(g + h), g(g), h(h), parent(parent) {}

    /**
     * Comparison operator for priority queue (higher cost = lower priority)
     */
    bool operator>(const Position &other) const
    {
        return cost > other.cost;
    }
};

/**
 * Calculates Manhattan distance between two points with wrap-around
 *
 * @param x1, y1 First point coordinates
 * @param x2, y2 Second point coordinates
 * @return Shortest Manhattan distance accounting for grid wrap-around
 */
float distanceWithWrap(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);

/**
 * Performs flood fill to count accessible cells from a position
 *
 * @param startX, startY Starting position for flood fill
 * @param tempGrid Grid representation with obstacles
 * @return Number of accessible cells
 */
int floodFill(uint8_t startX, uint8_t startY, bool tempGrid[][64]);

/**
 * Predicts an opponent's likely next move based on trajectory
 *
 * @param opponentIdx Index of the opponent (0-3)
 * @return Predicted direction (UP=1, RIGHT=2, DOWN=3, LEFT=4) or 0 if unknown
 */
uint8_t predictOpponentMove(int opponentIdx);

/**
 * Evaluates the quality of a potential move using heuristics
 *
 * @param x, y Current position
 * @param direction Direction to evaluate (UP=1, RIGHT=2, DOWN=3, LEFT=4)
 * @return Score for the move (higher is better)
 */
float evaluateMove(uint8_t x, uint8_t y, uint8_t direction);

/**
 * Processes game state updates and selects best move
 *
 * @param data Game state data received via CAN bus
 */
void process_GameState(uint8_t *data);

/**
 * Processes player death messages
 *
 * @param data Death message data
 */
void process_Die(uint8_t *data);
void process_GameFinish(uint8_t *data);

/**
 * Processes error messages from the server
 *
 * @param data Error message data containing error code
 */
void process_Error(uint8_t *data);

/**
 * Game state storage
 * grid: Represents occupied cells in the game (true = occupied)
 * player_traces: Stores movement history of all players
 */
extern bool grid[64][64];
extern std::vector<std::pair<uint8_t, uint8_t>> player_traces[4];

#endif