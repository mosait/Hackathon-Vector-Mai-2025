// Feather-m4-can_bot_example/include/Hackathon25.h
/**
 * @file Hackathon25.h
 * @brief Common definitions for the Vector Hackathon Tron game
 *
 * Defines:
 * - Game protocol message IDs and structures
 * - Global variables for player state
 * - Message format structures for protocol communication
 */

#ifndef HACKATHON25_H
#define HACKATHON25_H

#include <stdint.h>

/**
 * Global player variables
 * player_ID: Assigned by server, used in all communication (range: 1-4)
 * is_dead: Tracks whether our player has died in the current game
 */
extern uint8_t player_ID;
extern bool is_dead;

/**
 * CAN Message IDs used in the game protocol
 * These identify different message types in the CAN communication
 */
enum CAN_MSGs
{
    Join = 0x100,       // Join request from player
    Player = 0x110,     // Player ID assignment from server
    Game = 0x040,       // New game announcement
    GameAck = 0x120,    // Game participation acknowledgement
    GameState = 0x050,  // Game state update with player positions
    Move = 0x090,       // Movement direction command
    Die = 0x080,        // Player death notification
    GameFinish = 0x070, // Game end with points allocation
    Error = 0x020       // Error notification
};

/**
 * Structure for Join message
 * Sent by player to request joining the game
 */
struct __attribute__((packed)) MSG_Join
{
    uint32_t HardwareID; // Unique hardware identifier
};

/**
 * Structure for Player message
 * Sent by server to assign player ID to hardware ID
 */
struct __attribute__((packed)) MSG_Player
{
    uint32_t HardwareID; // Hardware ID being assigned
    uint8_t PlayerID;    // Assigned player ID (1-4)
};

#endif