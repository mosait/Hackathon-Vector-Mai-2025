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

void process_GameState(uint8_t *data);
void process_Die(uint8_t *data);
void process_GameFinish(uint8_t *data);
void process_Error(uint8_t *data);


#endif