// Feather-m4-can_bot_example/src/main.cpp
/**
 * @file main.cpp
 * @brief Main program entry point for the Vector Hackathon Tron game bot
 *
 * This file contains the Arduino setup and loop functions.
 * The program uses an event-driven architecture where the main processing
 * happens in response to CAN bus messages rather than in the main loop.
 */

#include <Arduino.h>
#include "CANHandler.h"
#include "GameLogic.h"

/**
 * Arduino setup function - runs once at program start
 *
 * Initializes hardware components and joins the game:
 * 1. Sets up serial communication for debugging
 * 2. Initializes CAN bus with appropriate settings
 * 3. Registers callback for incoming CAN messages
 * 4. Sends initial join request to the game server
 */
void setup()
{
    // Initialize serial for debugging output at 115200 baud
    Serial.begin(115200);
    // Uncomment to wait for serial monitor connection before continuing
    // while (!Serial);

    // Initialize CAN bus for communication with game server
    Serial.println("Initializing CAN bus...");
    if (!setupCan(500000)) // 500 kbps baud rate standard for automotive CAN
    {
        Serial.println("Error: CAN initialization failed!");
        while (1)
            ; // Halt execution if CAN initialization fails
    }
    Serial.println("CAN bus initialized successfully.");

    // Register callback function for incoming CAN messages
    // This is where most of the program's logic happens
    CAN.onReceive(onReceive);

    // Brief delay to ensure hardware is fully initialized
    delay(1000);

    // Send join request to the game server to participate
    send_Join();
}

/**
 * Arduino main loop - runs continuously after setup
 *
 * This function is intentionally left empty because the program uses
 * an event-driven approach where all processing happens in response to
 * CAN bus messages via the registered callback function.
 *
 * The CAN library automatically calls our onReceive function when
 * messages arrive, so we don't need to poll for messages in the loop.
 */
void loop()
{
    // Program logic is handled by CAN message callbacks
    // Keeping this empty improves responsiveness to incoming messages
}