// Feather-m4-can_bot_example/src/GameLogic.cpp
#include "GameLogic.h"
#include "CANHandler.h"
#include <queue>
#include <functional>
#include <algorithm>

// Constants for grid dimensions
const uint8_t GRID_WIDTH = 64;
const uint8_t GRID_HEIGHT = 64;

// Game state storage
bool grid[GRID_WIDTH][GRID_HEIGHT] = {false};              // Grid representation: true = occupied, false = free
std::vector<std::pair<uint8_t, uint8_t>> player_traces[4]; // Stores movement history of all players

// Movement state tracking
uint8_t last_direction = 1;  // Start with UP as default direction
uint8_t my_player_index = 0; // Will be set based on player_ID (player_ID - 1)

// Define direction constants and movement vectors
const int UP = 1, RIGHT = 2, DOWN = 3, LEFT = 4;
const int dx[] = {0, 1, 0, -1}; // Direction vectors for UP, RIGHT, DOWN, LEFT
const int dy[] = {-1, 0, 1, 0}; // Direction vectors for UP, RIGHT, DOWN, LEFT


int floodFill(uint8_t startX, uint8_t startY, bool tempGrid[][64])
{
    // Create a local copy of the grid to track visited cells
    bool visited[GRID_WIDTH][GRID_HEIGHT] = {false};

    std::queue<std::pair<uint8_t, uint8_t>> q;
    q.push({startX, startY});
    int count = 0;

    while (!q.empty())
    {
        auto [x, y] = q.front();
        q.pop();

        // Skip if already visited or occupied
        if (visited[x][y] || tempGrid[x][y])
            continue;

        // Mark as visited
        visited[x][y] = true;
        count++;

        // Explore all 4 directions
        for (int dir = 0; dir < 4; dir++)
        {
            uint8_t nx = (x + dx[dir] + GRID_WIDTH) % GRID_WIDTH;
            uint8_t ny = (y + dy[dir] + GRID_HEIGHT) % GRID_HEIGHT;

            if (!visited[nx][ny] && !tempGrid[nx][ny]) // Only add unvisited and free cells
                q.push({nx, ny});
        }
    }

    return count;
}

/**
 * Evaluates the quality of a potential move using simple rules
 *
 * @param x, y Current position of our player
 * @param direction Direction to evaluate (UP/RIGHT/DOWN/LEFT)
 * @return Score for the move (higher is better)
 */
float evaluateMove(uint8_t x, uint8_t y, uint8_t direction)
{
    uint8_t nx = (x + dx[direction - 1] + GRID_WIDTH) % GRID_WIDTH;
    uint8_t ny = (y + dy[direction - 1] + GRID_HEIGHT) % GRID_HEIGHT;

    if (grid[nx][ny])
    {
        return -1000.0f; // Collision penalty
    }

    bool tempGrid[GRID_WIDTH][GRID_HEIGHT];
    memcpy(tempGrid, grid, sizeof(grid));
    tempGrid[nx][ny] = true;
    int accessibleSpace = floodFill(nx, ny, tempGrid);

    float score = accessibleSpace * 10.0f; // Prioritize open space
    if (direction == last_direction)
        score += 5.0f; // Bonus for continuing in the same direction
    if (abs(direction - last_direction) == 2)
        score -= 2000.0f; // High penalty for 180° turns

    return score;
}

/**
 * Processes game state updates and selects the best move
 *
 * @param data Game state data received via CAN bus
 */
void process_GameState(uint8_t *data)
{
    // Parse player positions from data packet
    uint8_t player1_x = data[0];
    uint8_t player1_y = data[1];
    uint8_t player2_x = data[2];
    uint8_t player2_y = data[3];
    uint8_t player3_x = data[4];
    uint8_t player3_y = data[5];
    uint8_t player4_x = data[6];
    uint8_t player4_y = data[7];

    // Calculate our player index (0-3) from player_ID (1-4)
    my_player_index = player_ID - 1;

    // Reset the grid to start fresh
    memset(grid, false, sizeof(grid));

    // Lambda to update player positions and traces
    auto updatePlayerPosition = [&](uint8_t x, uint8_t y, int playerIndex)
    {
        if (x != 255 && y != 255) // Valid position (player is alive)
        {
            grid[x][y] = true; // Mark occupied cell
            player_traces[playerIndex].emplace_back(x, y); // Add to trace history
            Serial.printf("Player %d occupies (%u, %u)\n", playerIndex + 1, x, y); // Debug log
        }
    };

    // Update all player positions
    updatePlayerPosition(player1_x, player1_y, 0);
    updatePlayerPosition(player2_x, player2_y, 1);
    updatePlayerPosition(player3_x, player3_y, 2);
    updatePlayerPosition(player4_x, player4_y, 3);

    // Get our current position
    if (player_traces[my_player_index].empty())
    {
        Serial.println("ERROR: My player position not found!");
        return;
    }

    uint8_t myX = player_traces[my_player_index].back().first;
    uint8_t myY = player_traces[my_player_index].back().second;

    // Evaluate all four possible moves
    float best_score = -1000.0f;
    uint8_t best_direction = 0;

    for (uint8_t dir = 1; dir <= 4; dir++)
    {
        float score = evaluateMove(myX, myY, dir);
        Serial.printf("Direction: %u, Score: %.2f\n", dir, score); // Debug log

        if (score > best_score)
        {
            best_score = score;
            best_direction = dir;
        }
    }

    // Send the move command if we found a valid direction
    if (best_direction > 0)
    {
        Serial.printf("Best Direction: %u, Best Score: %.2f\n", best_direction, best_score); // Debug log
        send_Move(best_direction);
        last_direction = best_direction; // Update our direction tracking
    }
    else
    {
        Serial.println("No valid moves available. Attempting least risky move...");
        for (uint8_t dir = 1; dir <= 4; dir++)
        {
            if (evaluateMove(myX, myY, dir) > -1000.0f)
            {
                best_direction = dir;
                break;
            }
        }
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