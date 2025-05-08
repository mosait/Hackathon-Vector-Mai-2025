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

// Define direction constants and movement vectors for clarity
const int UP = 1, RIGHT = 2, DOWN = 3, LEFT = 4;
const int dx[] = {0, 1, 0, -1}; // Direction vectors for UP, RIGHT, DOWN, LEFT
const int dy[] = {-1, 0, 1, 0}; // Direction vectors for UP, RIGHT, DOWN, LEFT

/**
 * Calculates Manhattan distance between two points considering grid wrap-around
 *
 * @param x1, y1 First point coordinates
 * @param x2, y2 Second point coordinates
 * @return Manhattan distance accounting for grid wrap-around
 */
float distanceWithWrap(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2)
{
    // Calculate absolute differences
    int dx = abs((int)x1 - (int)x2);
    int dy = abs((int)y1 - (int)y2);

    // Consider wrap-around distance (shorter path may be across grid edges)
    dx = min(dx, GRID_WIDTH - dx);
    dy = min(dy, GRID_HEIGHT - dy);

    return dx + dy;
}

/**
 * Performs flood fill algorithm to determine accessible area from a position
 *
 * @param startX, startY Starting position for flood fill
 * @param tempGrid Grid representation to check for obstacles
 * @return Number of cells accessible from the starting position
 */
int floodFill(uint8_t startX, uint8_t startY, bool tempGrid[][GRID_HEIGHT])
{
    bool visited[GRID_WIDTH][GRID_HEIGHT] = {false}; // Track visited cells
    std::queue<std::pair<uint8_t, uint8_t>> q;       // Queue for BFS

    // Start from the given position
    q.push({startX, startY});
    visited[startX][startY] = true;
    int area = 0; // Counts accessible cells

    // Breadth-first search to visit all connected cells
    while (!q.empty())
    {
        // Using explicit pair access for C++14 compatibility
        std::pair<uint8_t, uint8_t> current = q.front();
        uint8_t x = current.first;
        uint8_t y = current.second;
        q.pop();
        area++; // Count this cell

        // Check all four adjacent directions
        for (int i = 0; i < 4; i++)
        {
            // Calculate next position with wrap-around
            uint8_t nx = (x + dx[i] + GRID_WIDTH) % GRID_WIDTH;
            uint8_t ny = (y + dy[i] + GRID_HEIGHT) % GRID_HEIGHT;

            // If not visited and not blocked, add to queue
            if (!visited[nx][ny] && !tempGrid[nx][ny])
            {
                visited[nx][ny] = true;
                q.push({nx, ny});
            }
        }
    }

    return area;
}

/**
 * Predicts an opponent's likely next move based on their current trajectory
 *
 * @param opponentIdx Index of the opponent (0-3)
 * @return Predicted direction (UP/RIGHT/DOWN/LEFT) or 0 if prediction not possible
 */
uint8_t predictOpponentMove(int opponentIdx)
{
    // Need at least two positions to determine direction
    if (player_traces[opponentIdx].size() < 2)
        return 0;

    // Get the opponent's last two positions
    auto &trace = player_traces[opponentIdx];
    uint8_t x1 = trace[trace.size() - 2].first;
    uint8_t y1 = trace[trace.size() - 2].second;
    uint8_t x2 = trace[trace.size() - 1].first;
    uint8_t y2 = trace[trace.size() - 1].second;

    // Determine current direction considering wrap-around
    uint8_t current_dir = 0;
    if (x2 == (x1 + 1) % GRID_WIDTH)
        current_dir = RIGHT;
    else if (x2 == (x1 + GRID_WIDTH - 1) % GRID_WIDTH)
        current_dir = LEFT;
    else if (y2 == (y1 + 1) % GRID_HEIGHT)
        current_dir = DOWN;
    else if (y2 == (y1 + GRID_HEIGHT - 1) % GRID_HEIGHT)
        current_dir = UP;

    // First, predict opponent will continue in the same direction
    uint8_t nx = (x2 + dx[current_dir - 1] + GRID_WIDTH) % GRID_WIDTH;
    uint8_t ny = (y2 + dy[current_dir - 1] + GRID_HEIGHT) % GRID_HEIGHT;

    if (!grid[nx][ny])
        return current_dir; // Can continue in same direction

    // If continuing is impossible, predict a turn (not 180° turn)
    for (int i = 1; i <= 4; i++)
    {
        if (abs(i - current_dir) == 2)
            continue; // Skip 180° turn (impossible in game)

        nx = (x2 + dx[i - 1] + GRID_WIDTH) % GRID_WIDTH;
        ny = (y2 + dy[i - 1] + GRID_HEIGHT) % GRID_HEIGHT;

        if (!grid[nx][ny])
            return i; // Found valid turn direction
    }

    return 0; // No valid move prediction (opponent is trapped)
}

/**
 * Evaluates the quality of a potential move using multiple heuristics
 *
 * @param x, y Current position of our player
 * @param direction Direction to evaluate (UP/RIGHT/DOWN/LEFT)
 * @return Score for the move (higher is better)
 */
float evaluateMove(uint8_t x, uint8_t y, uint8_t direction)
{
    // Calculate potential new position with wrap-around
    uint8_t nx = (x + dx[direction - 1] + GRID_WIDTH) % GRID_WIDTH;
    uint8_t ny = (y + dy[direction - 1] + GRID_HEIGHT) % GRID_HEIGHT;

    // Check if the move is valid (not hitting an occupied cell)
    if (grid[nx][ny])
        return -1000.0f; // Invalid move, assign very low score

    // Create temporary grid for simulation
    bool tempGrid[GRID_WIDTH][GRID_HEIGHT];
    memcpy(tempGrid, grid, sizeof(grid));
    tempGrid[nx][ny] = true; // Mark the new position as occupied

    // Calculate available space with flood fill (key territory metric)
    int availableSpace = floodFill(nx, ny, tempGrid);

    // Calculate distance to closest opponent (for collision avoidance)
    float minDistToOpponent = 1000.0f;
    for (int i = 0; i < 4; i++)
    {
        if (i == my_player_index || player_traces[i].empty())
            continue; // Skip self or dead players

        auto &opTrace = player_traces[i];
        float dist = distanceWithWrap(nx, ny, opTrace.back().first, opTrace.back().second);
        minDistToOpponent = min(minDistToOpponent, dist);
    }

    // Calculate wall-following score (preference for edge-following)
    int wallAdjacency = 0;
    for (int i = 0; i < 4; i++)
    {
        uint8_t wx = (nx + dx[i] + GRID_WIDTH) % GRID_WIDTH;
        uint8_t wy = (ny + dy[i] + GRID_HEIGHT) % GRID_HEIGHT;

        if (grid[wx][wy])
            wallAdjacency++;
    }

    // Weighted evaluation combining multiple factors
    float score =
        availableSpace * 10.0f +           // Available space is very important (×10)
        minDistToOpponent * 0.5f +         // Some distance from opponents is good (×0.5)
        (wallAdjacency > 0 ? 5.0f : 0.0f); // Slight preference for wall following

    // Check for illegal 180° turn (would cause immediate death)
    if (abs(direction - last_direction) == 2)
    {
        score -= 2000.0f; // Heavy penalty to avoid this move
    }

    // Prefer continued motion in the same direction (smoother paths)
    if (direction == last_direction)
    {
        score += 5.0f;
    }

    // Avoid collision with opponents' predicted paths
    for (int i = 0; i < 4; i++)
    {
        if (i == my_player_index || player_traces[i].empty())
            continue; // Skip self or dead players

        uint8_t predictedDir = predictOpponentMove(i);
        if (predictedDir > 0)
        {
            auto &opTrace = player_traces[i];
            uint8_t opX = opTrace.back().first;
            uint8_t opY = opTrace.back().second;

            // Calculate opponent's predicted next position
            uint8_t pnx = (opX + dx[predictedDir - 1] + GRID_WIDTH) % GRID_WIDTH;
            uint8_t pny = (opY + dy[predictedDir - 1] + GRID_HEIGHT) % GRID_HEIGHT;

            // Penalize potential direct collision
            if (nx == pnx && ny == pny)
            {
                score -= 200.0f;
            }
            // Penalize getting too close to opponent's predicted path
            else if (distanceWithWrap(nx, ny, pnx, pny) <= 1)
            {
                score -= 100.0f;
            }
        }
    }

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
        if (x != 255 && y != 255)
        {                                                  // Valid position (player is alive)
            grid[x][y] = true;                             // Mark occupied cell
            player_traces[playerIndex].emplace_back(x, y); // Add to trace history
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

        if (score > best_score)
        {
            best_score = score;
            best_direction = dir;
        }
    }

    // If no good move found or all moves score poorly, try emergency logic
    if (best_direction == 0 || best_score < -500.0f)
    {
        Serial.println("WARNING: No good moves found, trying emergency move!");

        // Simple emergency: find any valid direction that doesn't cause immediate death
        for (uint8_t dir = 1; dir <= 4; dir++)
        {
            // Skip 180° turns (instant death)
            if (abs(dir - last_direction) == 2)
                continue;

            uint8_t nx = (myX + dx[dir - 1] + GRID_WIDTH) % GRID_WIDTH;
            uint8_t ny = (myY + dy[dir - 1] + GRID_HEIGHT) % GRID_HEIGHT;

            if (!grid[nx][ny])
            {
                best_direction = dir;
                break;
            }
        }
    }

    // Send the move command if we found a valid direction
    if (best_direction > 0)
    {
        send_Move(best_direction);
        last_direction = best_direction; // Update our direction tracking
    }
    else
    {
        Serial.println("CRITICAL: No valid moves available!");
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

    // Clear traces of the dead player from the grid
    if (dead_player_id >= 1 && dead_player_id <= 4)
    {
        for (const auto &trace : player_traces[dead_player_id - 1])
        {
            grid[trace.first][trace.second] = false; // Free up the grid cell
        }
        player_traces[dead_player_id - 1].clear(); // Clear trace history
        Serial.printf("Traces for Player %u cleared.\n", dead_player_id);
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