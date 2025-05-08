#include "GameLogic.h"
#include "CANHandler.h"
#include <queue>
#include <functional>
#include <algorithm>

const uint8_t GRID_WIDTH = 64;
const uint8_t GRID_HEIGHT = 64;
bool grid[GRID_WIDTH][GRID_HEIGHT] = {false};
std::vector<std::pair<uint8_t, uint8_t>> player_traces[4];

// Hilfsfunktion: Berechne Manhattan-Distanz
float heuristic(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
    return abs(x1 - x2) + abs(y1 - y2);
}

// Hilfsfunktion: Finde den kürzesten Weg mit A*
std::vector<Position> findPath(uint8_t startX, uint8_t startY, uint8_t goalX, uint8_t goalY) {
    std::priority_queue<Position, std::vector<Position>, std::greater<Position>> openSet;
    bool closedSet[GRID_WIDTH][GRID_HEIGHT] = {false};

    openSet.emplace(startX, startY, 0, heuristic(startX, startY, goalX, goalY));


    while (!openSet.empty()) {
        Position current = openSet.top();
        openSet.pop();

        if (current.x == goalX && current.y == goalY) {
            // Ziel erreicht, Pfad zurückverfolgen
            std::vector<Position> path;
            for (Position* p = &current; p != nullptr; p = p->parent) {
                path.push_back(*p);
            }
            std::reverse(path.begin(), path.end());
            return path;
        }

        if (closedSet[current.x][current.y]) continue;
        closedSet[current.x][current.y] = true;

        // Nachbarn prüfen
        const int dx[] = {0, 1, 0, -1};
        const int dy[] = {-1, 0, 1, 0};
        for (int i = 0; i < 4; i++) {
            uint8_t nx = (current.x + dx[i] + GRID_WIDTH) % GRID_WIDTH; // Wrap-around
            uint8_t ny = (current.y + dy[i] + GRID_HEIGHT) % GRID_HEIGHT; // Wrap-around

            if (!grid[nx][ny] && !closedSet[nx][ny]) {
                float g = current.g + 1; // Kosten für Bewegung
                float h = heuristic(nx, ny, goalX, goalY);
                openSet.emplace(nx, ny, g, h, new Position(current));
            }
        }
    }

    // Kein Pfad gefunden
    return {};
}


// Hilfsfunktion: Zähle freien Platz um eine Position
int countFreeSpace(uint8_t x, uint8_t y) {
    int freeSpace = 0;
    const int dx[] = {0, 1, 0, -1};
    const int dy[] = {-1, 0, 1, 0};

    for (int i = 0; i < 4; i++) {
        uint8_t nx = (x + dx[i] + GRID_WIDTH) % GRID_WIDTH; // Wrap-around
        uint8_t ny = (y + dy[i] + GRID_HEIGHT) % GRID_HEIGHT; // Wrap-around
        if (!grid[nx][ny]) {
            freeSpace++;
        }
    }
    return freeSpace;
}



// Verbesserte Zielsetzung und Pfadfindung
void process_GameState(uint8_t* data) {
    uint8_t player1_x = data[0];
    uint8_t player1_y = data[1];
    uint8_t player2_x = data[2];
    uint8_t player2_y = data[3];
    uint8_t player3_x = data[4];
    uint8_t player3_y = data[5];
    uint8_t player4_x = data[6];
    uint8_t player4_y = data[7];

    // Hindernisse und Spielerpositionen aktualisieren
    memset(grid, false, sizeof(grid)); // Spielfeld zurücksetzen

    // Spielerpositionen und Traces aktualisieren
    auto updatePlayerPosition = [&](uint8_t x, uint8_t y, int playerIndex) {
        if (x != 255 && y != 255) {
            grid[x][y] = true;
            player_traces[playerIndex].emplace_back(x, y);
        }
    };

    updatePlayerPosition(player1_x, player1_y, 0);
    updatePlayerPosition(player2_x, player2_y, 1);
    updatePlayerPosition(player3_x, player3_y, 2);
    updatePlayerPosition(player4_x, player4_y, 3);

    // Verbesserte Zielsetzung: Suche nach dem besten Ziel
    uint8_t goalX = player1_x;
    uint8_t goalY = player1_y;
    float bestScore = -1;

    for (uint8_t x = 0; x < GRID_WIDTH; x++) {
        for (uint8_t y = 0; y < GRID_HEIGHT; y++) {
            if (!grid[x][y]) { // Freier Bereich
                int freeSpace = countFreeSpace(x, y);
                float distance = heuristic(player1_x, player1_y, x, y);
                float score = freeSpace - distance * 0.5; // Balance zwischen Platz und Distanz

                if (score > bestScore) {
                    // Prüfen, ob das Ziel erreichbar ist
                    std::vector<Position> testPath = findPath(player1_x, player1_y, x, y);
                    if (!testPath.empty()) {
                        bestScore = score;
                        goalX = x;
                        goalY = y;
                    }
                }
            }
        }
    }

    // A*-Pfad finden
    std::vector<Position> path = findPath(player1_x, player1_y, goalX, goalY);

    if (!path.empty() && path.size() > 1) {
        // Nächste Bewegung bestimmen
        Position nextMove = path[1]; // Der erste Schritt nach dem Start
        if (nextMove.x > player1_x) send_Move(2); // RIGHT
        else if (nextMove.x < player1_x) send_Move(4); // LEFT
        else if (nextMove.y > player1_y) send_Move(3); // DOWN
        else if (nextMove.y < player1_y) send_Move(1); // UP
    } else {
        // Keine Pfad gefunden, zufällige Bewegung als Fallback
        Serial.println("No path found! Making a random move.");
        send_Move(random(1, 5)); // Zufällige Richtung: 1=UP, 2=RIGHT, 3=DOWN, 4=LEFT
    }
}

void process_Die(uint8_t* data) {
    uint8_t dead_player_id = data[0];
    Serial.printf("Player %u died\n", dead_player_id);
  
    if (dead_player_id == player_ID) {
        Serial.println("You died! Game over.");
        is_dead = true; // Spieler ist tot, keine Nachrichten mehr senden
    }
  
    // Traces des gestorbenen Spielers freigeben
    if (dead_player_id >= 1 && dead_player_id <= 4) {
        for (const auto& trace : player_traces[dead_player_id - 1]) {
            grid[trace.first][trace.second] = false; // Bereich freigeben
        }
        player_traces[dead_player_id - 1].clear(); // Traces löschen
        Serial.printf("Traces for Player %u cleared.\n", dead_player_id);
    }
}

void process_GameFinish(uint8_t* data) {
    Serial.println("Game finished. Points distribution:");
    for (int i = 0; i < 4; i++) {
        uint8_t player_id = data[i * 2];
        uint8_t points = data[i * 2 + 1];
        Serial.printf("Player %u: %u points\n", player_id, points);
    }
  
    // Spielzustand zurücksetzen
    is_dead = false; // Spieler ist wieder aktiv
    memset(grid, false, sizeof(grid)); // Spielfeld zurücksetzen
    for (int i = 0; i < 4; i++) {
        player_traces[i].clear(); // Traces aller Spieler löschen
    }
  
    // Automatisch erneut dem Spiel beitreten
    Serial.println("Rejoining the game...");
    send_Join(); // Sende die `join`-Nachricht erneut
}


void process_Error(uint8_t* data) {
    uint8_t player_id = data[0];
    uint8_t error_code = data[1];
    Serial.printf("Error received: Player ID: %u, Error Code: %u\n", player_id, error_code);
  
    switch (error_code) {
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