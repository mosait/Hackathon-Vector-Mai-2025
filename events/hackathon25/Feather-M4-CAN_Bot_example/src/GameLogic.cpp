// Feather-m4-can_bot_example/src/GameLogic.cpp
#include "GameLogic.h"
#include "CANHandler.h"
#include <queue>
#include <cstring>

// Constants for grid dimensions
const uint8_t GRID_WIDTH = 64;
const uint8_t GRID_HEIGHT = 64;
std::vector<std::pair<uint8_t, uint8_t>> player_traces[4]; // Tracks movement history of all players

// Game state storage
bool grid[GRID_WIDTH][GRID_HEIGHT] = {false}; // Grid representation: true = occupied, false = free
uint8_t last_direction = 1;                  // Start with UP as default direction

// Direction vectors
const int dx[] = {0, 1, 0, -1}; // UP, RIGHT, DOWN, LEFT
const int dy[] = {-1, 0, 1, 0};

/**
 * Flood fill to calculate accessible area from a given position.
 *
 * @param x Starting x-coordinate
 * @param y Starting y-coordinate
 * @return Number of accessible cells
 */
int calculateAccessibleArea(uint8_t x, uint8_t y)
{
    bool visited[GRID_WIDTH][GRID_HEIGHT] = {false};
    std::queue<std::pair<uint8_t, uint8_t>> q;
    q.push({x, y});
    visited[x][y] = true;

    int area = 0;

    while (!q.empty())
    {
        auto [cx, cy] = q.front();
        q.pop();
        area++;

        for (int i = 0; i < 4; i++)
        {
            uint8_t nx = (cx + dx[i] + GRID_WIDTH) % GRID_WIDTH;
            uint8_t ny = (cy + dy[i] + GRID_HEIGHT) % GRID_HEIGHT;

            if (!visited[nx][ny] && !grid[nx][ny])
            {
                visited[nx][ny] = true;
                q.push({nx, ny});
            }
        }
    }

    return area;
}

/**
 * Evaluates a move based on collision avoidance and accessible area.
 *
 * @param x Current x-coordinate
 * @param y Current y-coordinate
 * @param direction Direction to evaluate (1=UP, 2=RIGHT, 3=DOWN, 4=LEFT)
 * @return Score for the move
 */
float evaluateMove(uint8_t x, uint8_t y, uint8_t direction)
{
    uint8_t nx = (x + dx[direction - 1] + GRID_WIDTH) % GRID_WIDTH;
    uint8_t ny = (y + dy[direction - 1] + GRID_HEIGHT) % GRID_HEIGHT;

    // Check for collision
    if (grid[nx][ny])
        return -1000.0f; // High penalty for collisions

    // Calculate accessible area
    return calculateAccessibleArea(nx, ny);
}

/**
 * Processes game state updates and selects the best move.
 *
 * @param data Game state data received via CAN bus
 */
void process_GameState(uint8_t *data)
{
    // Parse player positions
    uint8_t player_positions[4][2] = {
        {data[0], data[1]},
        {data[2], data[3]},
        {data[4], data[5]},
        {data[6], data[7]}};

    // Update grid
    memset(grid, false, sizeof(grid));
    for (int i = 0; i < 4; i++)
    {
        if (player_positions[i][0] != 255 && player_positions[i][1] != 255)
        {
            grid[player_positions[i][0]][player_positions[i][1]] = true;
        }
    }

    // Get our current position
    uint8_t myX = player_positions[player_ID - 1][0];
    uint8_t myY = player_positions[player_ID - 1][1];

    // Evaluate all possible moves
    float best_score = -1000.0f;
    uint8_t best_direction = 0;

    for (uint8_t dir = 1; dir <= 4; dir++)
    {
        float score = evaluateMove(myX, myY, dir);
        if (score > best_score)
        {
            best_score = score;
            best_direction = dir;
        }
    }

    // Send the best move
    if (best_direction > 0)
    {
        send_Move(best_direction);
        last_direction = best_direction;
    }
}

/**
 * Processes player death messages
 *
 * @param data Death message data
 */
void process_Die(uint8_t *data)
{
    uint8_t dead_player_id = data[0];
    Serial.printf("Player %u died\n", dead_player_id);

    if (dead_player_id == player_ID)
    {
        Serial.println("You died! Game over.");
        is_dead = true;
    }

    if (dead_player_id >= 1 && dead_player_id <= 4)
    {
        for (const auto &trace : player_traces[dead_player_id - 1])
        {
            grid[trace.first][trace.second] = false; // Free up the grid cell
            Serial.printf("Cleared trace at (%u, %u) for Player %u\n", trace.first, trace.second, dead_player_id);
        }
        player_traces[dead_player_id - 1].clear();
    }
}

/**
 * Processes game finish messages and resets game state
 *
 * @param data Game finish data containing points distribution
 */
void process_GameFinish(uint8_t *data)
{
    Serial.println("Game finished. Points distribution:");
    for (int i = 0; i < 4; i++)
    {
        uint8_t player_id = data[i * 2];
        uint8_t points = data[i * 2 + 1];
        Serial.printf("Player %u: %u points\n", player_id, points);
    }

    // Reset all game state for next game
    is_dead = false;
    memset(grid, false, sizeof(grid));
    for (int i = 0; i < 4; i++)
    {
        player_traces[i].clear();
    }
    last_direction = 1; // Reset to UP

    // Auto-rejoin for next game
    Serial.println("Rejoining the game...");
    send_Join();
}

/**
 * Processes error messages from the server
 *
 * @param data Error message data containing error code
 */
void process_Error(uint8_t *data)
{
    uint8_t player_id = data[0];
    uint8_t error_code = data[1];
    Serial.printf("Error received: Player ID: %u, Error Code: %u\n", player_id, error_code);

    // Handle various error types
    switch (error_code)
    {
    case 1:
        Serial.println("ERROR_INVALID_PLAYER_ID: Invalid Player ID.");
        break;
    case 2:
        Serial.println("ERROR_UNALLOWED_RENAME: Rename not allowed.");
        break;
    case 3:
        Serial.println("ERROR_YOU_ARE_NOT_PLAYING: Player is not in the game.");
        break;
    case 4:
        Serial.println("WARNING_UNKNOWN_MOVE: Invalid move direction.");
        break;
    default:
        Serial.println("Unknown error.");
        break;
    }
}