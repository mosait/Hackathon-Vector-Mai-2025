// Feather-m4-can_bot_example/include/CANHandler.h
/**
 * @file CANHandler.h
 * @brief Header for CAN bus communication handling
 *
 * Defines functions and interfaces for:
 * - CAN bus hardware setup
 * - Message sending and receiving over the CAN bus
 * - Communication protocol implementation for the Vector Hackathon Tron game
 */

#ifndef CAN_HANDLER_H
#define CAN_HANDLER_H

#include <Arduino.h>
#include <CAN.h>
#include "Hackathon25.h"

/**
 * Initializes the CAN bus hardware and transceiver
 *
 * @param baudRate CAN bus speed (typically 500000 for 500 kbps)
 * @return true if initialization successful, false otherwise
 */
bool setupCan(long baudRate);

/**
 * Callback function for handling incoming CAN messages
 * Registered with CAN library to be called when messages arrive
 *
 * @param packetSize Size of received CAN packet in bytes
 */
void onReceive(int packetSize);

/**
 * Sends a join request to the game server
 * This is the first message sent to participate in games
 */
void send_Join();

/**
 * Acknowledges participation in a game
 * Sent in response to a Game message from the server
 */
void send_GameAck();

/**
 * Sends a move command to change direction
 *
 * @param direction New movement direction (UP=1, RIGHT=2, DOWN=3, LEFT=4)
 */
void send_Move(uint8_t direction);

/**
 * Sends first part of player name for visualization
 *
 * @param name Player name (first 6 characters)
 * @param size Total name length (up to 20)
 */
void send_Rename(const char *name, uint8_t size);

/**
 * Sends remaining part of player name
 *
 * @param name Remaining characters of player name (up to 7)
 */
void send_RenameFollow(const char *name);

/**
 * Processes player ID assignment from server
 * Called when receiving a Player message
 */
void rcv_Player();

#endif