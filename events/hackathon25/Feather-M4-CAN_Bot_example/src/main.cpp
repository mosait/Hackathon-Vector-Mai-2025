#include <Arduino.h>
#include "CANHandler.h"
#include "GameLogic.h"

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
    send_Join();
}

// Loop remains empty, logic is event-driven via CAN callback
void loop() {}