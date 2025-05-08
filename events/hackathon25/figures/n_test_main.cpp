// Tron Game Client for Arduino using CAN bus
// Improved: Reliable Join/GameAck logic, strategic movement, dead player handling, space evaluation

#include <Arduino.h>
#include <CAN.h>
#include "Hackathon25.h"
#include <vector>
#include <queue>
#include <algorithm>
#include <cmath>

struct Position {
    uint8_t x, y;
    float g, h;
    float cost;
    Position* parent;

    Position(uint8_t x, uint8_t y, float g, float h, Position* parent = nullptr)
        : x(x), y(y), g(g), h(h), cost(g + h), parent(parent) {}

    bool operator>(const Position& other) const {
        return cost > other.cost;
    }
};

const uint8_t GRID_WIDTH = 64;
const uint8_t GRID_HEIGHT = 64;
bool grid[GRID_WIDTH][GRID_HEIGHT] = {false};
std::vector<std::pair<uint8_t, uint8_t>> player_traces[4];

const uint32_t hardware_ID = (*(RoReg *)0x008061FCUL);
uint8_t player_ID = 0;
uint8_t game_ID = 0;
bool is_dead = false;
bool game_ack_sent = false;
bool player_id_received = false;

// Movement memory
uint8_t last_direction = 1; // UP by default

// Function Prototypes
void send_Join();
void send_GameAck();
void send_Move(uint8_t direction);
void send_Rename(const char* name, uint8_t size);
void send_RenameFollow(const char* name);

void process_GameState(uint8_t* data);
void process_Die(uint8_t* data);
void process_GameFinish(uint8_t* data);
void process_Error(uint8_t* data);

float heuristic(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
    return abs(x1 - x2) + abs(y1 - y2);
}

std::vector<Position> findPath(uint8_t sx, uint8_t sy, uint8_t gx, uint8_t gy) {
    std::priority_queue<Position, std::vector<Position>, std::greater<Position>> open;
    bool closed[GRID_WIDTH][GRID_HEIGHT] = {false};
    open.emplace(sx, sy, 0, heuristic(sx, sy, gx, gy));

    while (!open.empty()) {
        Position current = open.top(); open.pop();
        if (current.x == gx && current.y == gy) {
            std::vector<Position> path;
            for (Position* p = &current; p != nullptr; p = p->parent)
                path.push_back(*p);
            std::reverse(path.begin(), path.end());
            return path;
        }
        if (closed[current.x][current.y]) continue;
        closed[current.x][current.y] = true;

        const int dx[] = {0, 1, 0, -1};
        const int dy[] = {-1, 0, 1, 0};
        for (int i = 0; i < 4; ++i) {
            uint8_t nx = (current.x + dx[i] + GRID_WIDTH) % GRID_WIDTH;
            uint8_t ny = (current.y + dy[i] + GRID_HEIGHT) % GRID_HEIGHT;
            if (!grid[nx][ny] && !closed[nx][ny]) {
                open.emplace(nx, ny, current.g + 1, heuristic(nx, ny, gx, gy), new Position(current));
            }
        }
    }
    return {};
}

int countFreeSpace(uint8_t x, uint8_t y) {
    int free = 0;
    const int dx[] = {0, 1, 0, -1};
    const int dy[] = {-1, 0, 1, 0};
    for (int i = 0; i < 4; ++i) {
        uint8_t nx = (x + dx[i] + GRID_WIDTH) % GRID_WIDTH;
        uint8_t ny = (y + dy[i] + GRID_HEIGHT) % GRID_HEIGHT;
        if (!grid[nx][ny]) free++;
    }
    return free;
}

// CAN Communication
void onReceive(int packetSize) {
    if (!packetSize) return;
    uint8_t id = CAN.packetId();
    uint8_t data[8] = {0};
    CAN.readBytes(data, packetSize);

    switch (id) {
        case Player:
            if (*(uint32_t*)data == hardware_ID) {
                player_ID = data[4];
                player_id_received = true;
                send_Rename("sucuk_", 6);
                send_RenameFollow("mafia");
            }
            break;
        case Game:
            if (!game_ack_sent && player_id_received) {
                send_GameAck();
                game_ack_sent = true;
            }
            break;
        case GameState:
            if (!is_dead) process_GameState(data);
            break;
        case Die:
            process_Die(data);
            break;
        case GameFinish:
            process_GameFinish(data);
            break;
        case Error:
            process_Error(data);
            break;
    }
}

bool setupCan(long baudRate) {
    pinMode(PIN_CAN_STANDBY, OUTPUT);
    digitalWrite(PIN_CAN_STANDBY, LOW);
    pinMode(PIN_CAN_BOOSTEN, OUTPUT);
    digitalWrite(PIN_CAN_BOOSTEN, HIGH);
    return CAN.begin(baudRate);
}

void setup() {
    Serial.begin(115200);
    while (!Serial);
    if (!setupCan(500000)) while (1);
    CAN.onReceive(onReceive);
    send_Join();
}

void loop() {}

// Core Functions
void send_Join() {
    MSG_Join j;
    j.HardwareID = hardware_ID;
    CAN.beginPacket(Join);
    CAN.write((uint8_t*)&j, sizeof(j));
    CAN.endPacket();
}

void send_GameAck() {
    CAN.beginPacket(GameAck);
    CAN.write(player_ID);
    CAN.endPacket();
}

void send_Move(uint8_t dir) {
    if (dir == 0 || dir == last_direction || is_dead) return;
    // prevent direct opposite
    if ((last_direction == 1 && dir == 3) || (last_direction == 3 && dir == 1) ||
        (last_direction == 2 && dir == 4) || (last_direction == 4 && dir == 2)) return;
    CAN.beginPacket(Move);
    CAN.write(player_ID);
    CAN.write(dir);
    CAN.endPacket();
    last_direction = dir;
}

void send_Rename(const char* name, uint8_t size) {
    CAN.beginPacket(0x500);
    CAN.write(player_ID);
    CAN.write(size);
    CAN.write((uint8_t*)name, size);
    CAN.endPacket();
}

void send_RenameFollow(const char* name) {
    CAN.beginPacket(0x510);
    CAN.write(player_ID);
    CAN.write((uint8_t*)name, 7);
    CAN.endPacket();
}

void process_GameState(uint8_t* data) {
    uint8_t px[4], py[4];
    for (int i = 0; i < 4; ++i) {
        px[i] = data[i * 2];
        py[i] = data[i * 2 + 1];
        if (px[i] != 255 && py[i] != 255) {
            grid[px[i]][py[i]] = true;
            player_traces[i].emplace_back(px[i], py[i]);
        }
    }
    uint8_t sx = px[player_ID - 1];
    uint8_t sy = py[player_ID - 1];
    if (sx == 255 || sy == 255) return;

    uint8_t bestX = sx, bestY = sy;
    int maxSpace = -1;
    for (uint8_t x = 0; x < GRID_WIDTH; ++x) {
        for (uint8_t y = 0; y < GRID_HEIGHT; ++y) {
            if (!grid[x][y]) {
                int space = countFreeSpace(x, y);
                if (space > maxSpace) {
                    bestX = x;
                    bestY = y;
                    maxSpace = space;
                }
            }
        }
    }

    auto path = findPath(sx, sy, bestX, bestY);
    if (path.size() >= 2) {
        auto next = path[1];
        if (next.x > sx) send_Move(2);
        else if (next.x < sx) send_Move(4);
        else if (next.y > sy) send_Move(3);
        else if (next.y < sy) send_Move(1);
    } else {
        send_Move((random(1, 5))); // Fallback
    }
}

void process_Die(uint8_t* data) {
    uint8_t id = data[0];
    if (id == player_ID) is_dead = true;
    if (id >= 1 && id <= 4) {
        for (auto& pos : player_traces[id - 1])
            grid[pos.first][pos.second] = false;
        player_traces[id - 1].clear();
    }
}

void process_GameFinish(uint8_t* data) {
    is_dead = false;
    last_direction = 1;
    game_ack_sent = false;
    memset(grid, 0, sizeof(grid));
    for (int i = 0; i < 4; ++i) player_traces[i].clear();
    send_Join();
}

void process_Error(uint8_t* data) {
    Serial.printf("Error: Player %u, Code %u\n", data[0], data[1]);
    if (data[1] == 1) send_Join();
}
