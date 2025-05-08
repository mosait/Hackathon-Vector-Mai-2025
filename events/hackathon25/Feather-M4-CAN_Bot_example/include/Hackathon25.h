#ifndef HACKATHON25_H
#define HACKATHON25_H

#include <stdint.h>

extern uint8_t player_ID;
extern bool is_dead;

// CAN Message IDs
enum CAN_MSGs {
    Join = 0x100,
    Player = 0x110,
    Game = 0x040,
    GameAck = 0x120,
    GameState = 0x050,
    Move = 0x090,
    Die = 0x080,
    GameFinish = 0x070,
    Error = 0x020
};

struct __attribute__((packed)) MSG_Join {
    uint32_t HardwareID;
};

struct __attribute__((packed)) MSG_Player {
    uint32_t HardwareID;
    uint8_t PlayerID;
};


#endif