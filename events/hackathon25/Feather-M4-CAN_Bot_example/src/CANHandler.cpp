#include "GameLogic.h"
#include "CANHandler.h"

const uint32_t hardware_ID = (*(RoReg *)0x008061FCUL);
uint8_t player_ID = 0;
bool is_dead = false;

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

void onReceive(int packetSize) {
    if (packetSize) {
        switch (CAN.packetId()) {
            case Player:
                if (!is_dead) rcv_Player();
                break;
            case Game:
                is_dead = false;
                send_GameAck();
                break;
            case GameState:
                if (!is_dead) {
                    uint8_t data[8];
                    CAN.readBytes(data, sizeof(data));
                    process_GameState(data);
                }
                break;
            case Die:
                uint8_t die_data[1];
                CAN.readBytes(die_data, sizeof(die_data));
                process_Die(die_data);
                break;
            case GameFinish:
                uint8_t finish_data[8];
                CAN.readBytes(finish_data, sizeof(finish_data));
                process_GameFinish(finish_data);
                break;
            case Error:
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

void send_Join() {
    MSG_Join msg_join;
    msg_join.HardwareID = hardware_ID;

    CAN.beginPacket(Join);
    CAN.write((uint8_t*)&msg_join, sizeof(MSG_Join));
    CAN.endPacket();

    Serial.printf("JOIN packet sent (Hardware ID: %u)\n", hardware_ID);
}

void send_GameAck() {
    CAN.beginPacket(GameAck);
    CAN.write(player_ID);
    CAN.endPacket();
    Serial.printf("GameAck sent for Player ID: %u\n", player_ID);
}

void send_Move(uint8_t direction) {
    if (is_dead) return;

    // Verhindere Bewegung in ein belegtes Feld
    uint8_t nx = player_traces[player_ID - 1].back().first;
    uint8_t ny = player_traces[player_ID - 1].back().second;

    switch (direction) {
        case 1: ny--; break; // UP
        case 2: nx++; break; // RIGHT
        case 3: ny++; break; // DOWN
        case 4: nx--; break; // LEFT
    }

    if (grid[nx][ny]) {
        Serial.println("Invalid move: Field is occupied.");
        return;
    }

    CAN.beginPacket(Move);
    CAN.write(player_ID);
    CAN.write(direction);
    CAN.endPacket();
    Serial.printf("Move sent: Player ID: %u, Direction: %u\n", player_ID, direction);
}

void send_Rename(const char* name, uint8_t size) {
    CAN.beginPacket(0x500);
    CAN.write(player_ID);
    CAN.write(size);
    CAN.write((uint8_t*)name, 6);
    CAN.endPacket();
    Serial.printf("Rename sent: Player ID: %u, Name: %.6s\n", player_ID, name);
}

void send_RenameFollow(const char* name) {
    CAN.beginPacket(0x510);
    CAN.write(player_ID);
    CAN.write((uint8_t*)name, 7);
    CAN.endPacket();
    Serial.printf("RenameFollow sent: Player ID: %u, Name: %.7s\n", player_ID, name);
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