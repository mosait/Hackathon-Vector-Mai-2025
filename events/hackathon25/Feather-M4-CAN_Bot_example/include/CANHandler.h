#ifndef CAN_HANDLER_H
#define CAN_HANDLER_H

#include <Arduino.h>
#include <CAN.h>
#include "Hackathon25.h"

// CAN setup
bool setupCan(long baudRate);

// CAN message handlers
void onReceive(int packetSize);
void send_Join();
void send_GameAck();
void send_Move(uint8_t direction);
void send_Rename(const char* name, uint8_t size);
void send_RenameFollow(const char* name);
void rcv_Player();

#endif