/**
 * @file CANHandler.cpp
 * @brief CAN bus communication handling for Vector Hackathon Tron game
 *
 * Implements functions for:
 * - Setting up the CAN bus hardware
 * - Receiving and processing different types of CAN messages
 * - Sending various game commands via CAN
 * - Managing player identification and status
 */

#include "GameLogic.h"
#include "CANHandler.h"

/**
 * Hardware ID from device-specific register - unique identifier for this device
 * Used by the game server to distinguish between different player controllers
 */
const uint32_t hardware_ID = (*(RoReg *)0x008061FCUL);

/**
 * Global player variables
 * player_ID: Assigned by server, used in all communication (range: 1-4)
 * is_dead: Tracks whether our player has died in the current game
 */
uint8_t player_ID = 0; // Will be assigned by server after joining
bool is_dead = false;  // Initially alive

/**
 * Initializes the CAN bus hardware and transceiver
 *
 * @param baudRate CAN bus speed (typically 500000 for 500 kbps)
 * @return true if initialization successful, false otherwise
 */
bool setupCan(long baudRate)
{
    // Configure standby pin for CAN transceiver
    pinMode(PIN_CAN_STANDBY, OUTPUT);
    digitalWrite(PIN_CAN_STANDBY, false); // Disable standby mode

    // Configure signal boost pin for CAN transceiver
    pinMode(PIN_CAN_BOOSTEN, OUTPUT);
    digitalWrite(PIN_CAN_BOOSTEN, true); // Enable signal boost for reliable communication

    // Initialize CAN bus with specified baud rate
    if (!CAN.begin(baudRate))
    {
        return false; // Return false if initialization fails
    }
    return true;
}

/**
 * Callback function for handling incoming CAN messages
 * This is registered with the CAN library and called automatically when messages arrive
 *
 * @param packetSize Size of received CAN packet in bytes
 */
void onReceive(int packetSize)
{
    if (packetSize) // Only process if we actually received data
    {
        // Dispatch based on the CAN message ID
        switch (CAN.packetId())
        {
        case Player: // Player ID assignment from server
            if (!is_dead)
                rcv_Player();
            break;

        case Game:           // New game announcement
            is_dead = false; // Reset dead status at start of new game
            send_GameAck();  // Acknowledge game participation
            break;

        case GameState:   // Regular game state update (player positions)
            if (!is_dead) // Only process if our player is still alive
            {
                uint8_t data[8];
                CAN.readBytes(data, sizeof(data));
                process_GameState(data); // Process game state and choose next move
            }
            break;

        case Die: // Player death notification
            uint8_t die_data[1];
            CAN.readBytes(die_data, sizeof(die_data));
            process_Die(die_data); // Process player death
            break;

        case GameFinish: // Game end notification with points
            uint8_t finish_data[8];
            CAN.readBytes(finish_data, sizeof(finish_data));
            process_GameFinish(finish_data); // Process game end and prepare for next game
            break;

        case Error: // Error message from server
            uint8_t error_data[2];
            CAN.readBytes(error_data, sizeof(error_data));
            process_Error(error_data); // Handle error conditions
            break;

        default:
            Serial.println("CAN: Received unknown packet");
            break;
        }
    }
}

/**
 * Sends a join request to the game server
 * This is the first message sent to participate in games
 */
void send_Join()
{
    // Prepare join message with our hardware ID
    MSG_Join msg_join;
    msg_join.HardwareID = hardware_ID;

    // Send join request via CAN bus
    CAN.beginPacket(Join);
    CAN.write((uint8_t *)&msg_join, sizeof(MSG_Join));
    CAN.endPacket();

    Serial.printf("JOIN packet sent (Hardware ID: %u)\n", hardware_ID);
}

/**
 * Acknowledges participation in a game
 * Sent in response to a Game message from the server
 */
void send_GameAck()
{
    // Send acknowledgement with our assigned player ID
    CAN.beginPacket(GameAck);
    CAN.write(player_ID);
    CAN.endPacket();

    Serial.printf("GameAck sent for Player ID: %u\n", player_ID);
}

/**
 * Sends a move command to change direction
 *
 * @param direction New movement direction (UP=1, RIGHT=2, DOWN=3, LEFT=4)
 */
void send_Move(uint8_t direction)
{
    // Don't send moves if player is dead
    if (is_dead)
        return;

    // Send move command with player ID and direction
    CAN.beginPacket(Move);
    CAN.write(player_ID);
    CAN.write(direction);
    CAN.endPacket();

    Serial.printf("Move sent: Player ID: %u, Direction: %u\n", player_ID, direction);
}

/**
 * Sends first part of player name for visualization
 * The protocol allows names up to 20 characters, split across two messages
 *
 * @param name Player name (first 6 characters)
 * @param size Total name length (up to 20)
 */
void send_Rename(const char *name, uint8_t size)
{
    // Send first part of name (up to 6 characters)
    CAN.beginPacket(0x500); // Rename message ID
    CAN.write(player_ID);
    CAN.write(size);               // Total name length
    CAN.write((uint8_t *)name, 6); // First 6 characters
    CAN.endPacket();

    Serial.printf("Rename sent: Player ID: %u, Name: %.6s\n", player_ID, name);
}

/**
 * Sends remaining part of player name
 *
 * @param name Remaining characters of player name (up to 7)
 */
void send_RenameFollow(const char *name)
{
    // Send second part of name (up to 7 more characters)
    CAN.beginPacket(0x510); // RenameFollow message ID
    CAN.write(player_ID);
    CAN.write((uint8_t *)name, 7); // Up to 7 more characters
    CAN.endPacket();

    Serial.printf("RenameFollow sent: Player ID: %u, Name: %.7s\n", player_ID, name);
}

/**
 * Processes player ID assignment from server
 * Called when receiving a Player message
 */
void rcv_Player()
{
    // Read player assignment data
    MSG_Player msg_player;
    CAN.readBytes((uint8_t *)&msg_player, sizeof(MSG_Player));

    // Only accept player ID if hardware ID matches our device
    if (msg_player.HardwareID == hardware_ID)
    {
        player_ID = msg_player.PlayerID;
        Serial.printf("Player ID received: %u\n", player_ID);

        // Set team name for visualization in the game
        send_Rename("sucuk_", 12);  // Send first part of name (6 chars)
        send_RenameFollow("mafia"); // Send remaining part of name (5 chars)
    }

    // Log received player assignment details
    Serial.printf("Received Player packet | Player ID received: %u | Own Player ID: %u | Hardware ID received: %u | Own Hardware ID: %u\n",
                  msg_player.PlayerID, player_ID, msg_player.HardwareID, hardware_ID);
}