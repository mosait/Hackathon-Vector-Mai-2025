#include <Arduino.h>
#include <CAN.h>
#include "Hackathon25.h"

// Global variables
const uint32_t hardware_ID = (*(RoReg *)0x008061FCUL);
uint8_t player_ID = 0;
uint8_t game_ID = 0;
bool is_dead; 


// Function prototypes
void send_Join();
void rcv_Player();
void send_GameAck();
void process_GameState(uint8_t* data);
void process_Error(uint8_t* data);
void process_Die(uint8_t* data);
void process_GameFinish(uint8_t* data);
void send_Move(uint8_t direction);
void send_Rename(const char* name, uint8_t size);
void send_RenameFollow(const char* name);
void onReceive(int packetSize);

// CAN receive callback
void onReceive(int packetSize) {
  if (packetSize) {
      switch (CAN.packetId()) {
          case Player:
              Serial.println("CAN: Received Player packet");
              rcv_Player();
              break;
          case Game:
              Serial.println("CAN: Received Game packet");
              send_GameAck();
              break;
          case GameState:
              Serial.println("CAN: Received GameState packet");
              uint8_t data[8];
              CAN.readBytes(data, sizeof(data));
              process_GameState(data);
              break;
          case Die:
              Serial.println("CAN: Received Die packet");
              uint8_t die_data[1];
              CAN.readBytes(die_data, sizeof(die_data));
              process_Die(die_data);
              break;
          case GameFinish:
              Serial.println("CAN: Received GameFinish packet");
              uint8_t finish_data[8];
              CAN.readBytes(finish_data, sizeof(finish_data));
              process_GameFinish(finish_data);
              break;
          case Error:
              Serial.println("CAN: Received Error packet");
              uint8_t error_data[2];
              CAN.readBytes(error_data, sizeof(error_data));
              process_Error(error_data);
              break;
          default:
              Serial.println("CAN: Received unknown packet");
              break;
      }
  }
}

// CAN setup
bool setupCan(long baudRate) {
    pinMode(PIN_CAN_STANDBY, OUTPUT);
    digitalWrite(PIN_CAN_STANDBY, false);
    pinMode(PIN_CAN_BOOSTEN, OUTPUT);
    digitalWrite(PIN_CAN_BOOSTEN, true);

    if (!CAN.begin(baudRate)) {
        return false;
    }
    return true;
}

// Setup
void setup() {
    Serial.begin(115200);
    //while (!Serial);
    
    Serial.println("Initializing CAN bus...");
    if (!setupCan(500000)) {
        Serial.println("Error: CAN initialization failed!");
        while (1);
    }
    Serial.println("CAN bus initialized successfully."); 

    CAN.onReceive(onReceive);

    delay(1000);
    bool is_dead = false;
    send_Join();
}

// Loop remains empty, logic is event-driven via CAN callback
void loop() {}

// Send JOIN packet via CAN
void send_Join(){
    MSG_Join msg_join;
    msg_join.HardwareID = hardware_ID;

    CAN.beginPacket(Join);
    CAN.write((uint8_t*)&msg_join, sizeof(MSG_Join));
    CAN.endPacket();

    Serial.printf("JOIN packet sent (Hardware ID: %u)\n", hardware_ID);
}

void send_GameAck() {
  if (is_dead) {
    Serial.println("Cannot send GameAck: Player is dead.");
    return; // Nachricht nicht senden
  }

  CAN.beginPacket(GameAck);
  CAN.write(player_ID); // Spieler-ID senden
  CAN.endPacket();
  Serial.printf("GameAck sent for Player ID: %u\n", player_ID);
}

// Receive player information
void rcv_Player(){
    MSG_Player msg_player;
    CAN.readBytes((uint8_t*)&msg_player, sizeof(MSG_Player));

    if(msg_player.HardwareID == hardware_ID){
        player_ID = msg_player.PlayerID;
        Serial.printf("Player ID recieved\n");
    }
    //  else {
    //     player_ID = 0;
    // }

    send_Rename("sucuk_", 6); // Send first 6 characters of the name
    send_RenameFollow("mafia"); // Send next 7 characters of the name

    Serial.printf("Received Player packet | Player ID received: %u | Own Player ID: %u | Hardware ID received: %u | Own Hardware ID: %u\n", 
        msg_player.PlayerID, player_ID, msg_player.HardwareID, hardware_ID);
}

void send_Move(uint8_t direction) {
  if (is_dead) {
    Serial.println("Cannot send move: Player is dead.");
    return; // Nachricht nicht senden
  }

  CAN.beginPacket(Move);
  CAN.write(player_ID); // Spieler-ID
  CAN.write(direction); // Richtung: 1=UP, 2=RIGHT, 3=DOWN, 4=LEFT
  CAN.endPacket();
  Serial.printf("Move sent: Player ID: %u, Direction: %u\n", player_ID, direction);
}

void process_GameState(uint8_t* data) {
  uint8_t player1_x = data[0];
  uint8_t player1_y = data[1];
  uint8_t player2_x = data[2];
  uint8_t player2_y = data[3];
  uint8_t player3_x = data[4];
  uint8_t player3_y = data[5];
  uint8_t player4_x = data[6];
  uint8_t player4_y = data[7];

  Serial.printf("GameState received: P1(%u,%u), P2(%u,%u), P3(%u,%u), P4(%u,%u)\n",
                player1_x, player1_y, player2_x, player2_y, player3_x, player3_y, player4_x, player4_y);

  // Beispiel: Bewegung nach rechts senden
  send_Move(2); // 2 = RIGHT
}

void send_Rename(const char* name, uint8_t size) {
  if (is_dead) {
    Serial.println("Cannot send Rename: Player is dead.");
    return; // Nachricht nicht senden
  }

  CAN.beginPacket(0x500); // FrameID for rename
  CAN.write(player_ID);
  CAN.write(size);
  CAN.write((uint8_t*)name, 6); // First 6 characters
  CAN.endPacket();
  Serial.printf("Rename sent: Player ID: %u, Name: %.6s\n", player_ID, name);
}

void send_RenameFollow(const char* name) {
  if (is_dead) {
    Serial.println("Cannot send RenameFollow: Player is dead.");
    return; // Nachricht nicht senden
  }

  CAN.beginPacket(0x510); // FrameID for renamefollow
  CAN.write(player_ID);
  CAN.write((uint8_t*)name, 7); // Next 7 characters
  CAN.endPacket();
  Serial.printf("RenameFollow sent: Player ID: %u, Name: %.7s\n", player_ID, name);
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

void process_Die(uint8_t* data) {
  uint8_t dead_player_id = data[0];
  Serial.printf("Player %u died\n", dead_player_id);

  if (dead_player_id == player_ID) {
      Serial.println("You died! Game over.");
      is_dead = true; // Spieler ist tot, keine Nachrichten mehr senden
  }
}

void process_GameFinish(uint8_t* data) {
  Serial.println("Game finished. Points distribution:");
  for (int i = 0; i < 4; i++) {
      uint8_t player_id = data[i * 2];
      uint8_t points = data[i * 2 + 1];
      Serial.printf("Player %u: %u points\n", player_id, points);
  }
}