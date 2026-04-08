#include <Arduino.h>
#include <SPI.h>
#include <RH_RF95.h> // The powerful, raw Point-to-Point LoRa Library

// -----------------------------------------------------------------------------
// DFRobot SEN0311 (A02YYUW) Ultrasonic Sensor Config
// -----------------------------------------------------------------------------
// Set this to 'true' to fake distance readings on nodes without physical sensors!
#define MOCK_SENSOR_MODE true 

#define SENSOR_SERIAL Serial1
uint16_t currentDistance = 0;

// -----------------------------------------------------------------------------
// RadioHead LoRa Configuration (Feather M0)
// -----------------------------------------------------------------------------
#define RFM95_CS    8
#define RFM95_RST   4
#define RFM95_INT   3

#define RF95_FREQ 915.0

RH_RF95 rf95(RFM95_CS, RFM95_INT);

// -----------------------------------------------------------------------------
// MESH ROUTING ARCHITECTURE
// -----------------------------------------------------------------------------
// Assign this specific Feather board a unique Node ID. 
// Do NOT use 1 (The Gateway Pi is 1). Use 2, 3, 4, etc.
#define MY_NODE_ID 2
#define GATEWAY_ID 1

// Mesh tracker: Prevents infinite looping storms by remembering the last heard ID
uint8_t seen_messages[256] = {0}; 
uint8_t my_message_sequence = 0;  // Tracks our outgoing sequence ID

unsigned long lastTransmitTime = 0;
const unsigned long TX_INTERVAL_MS = 10000; // 10-second intervals

// -----------------------------------------------------------------------------
// Asynchronous Sensor Decoder
// -----------------------------------------------------------------------------
void updateSensorReading() {
  if (SENSOR_SERIAL.available() > 0) {
    if (SENSOR_SERIAL.peek() != 0xFF) {
      SENSOR_SERIAL.read(); // Discard trailing garbage byte
    } else if (SENSOR_SERIAL.available() >= 4) {
      uint8_t h  = SENSOR_SERIAL.read(); // Header: 0xFF
      uint8_t dH = SENSOR_SERIAL.read();
      uint8_t dL = SENSOR_SERIAL.read();
      uint8_t cs = SENSOR_SERIAL.read();
      
      if (((h + dH + dL) & 0xFF) == cs) {
        uint16_t dist = (dH << 8) | dL;
        if (dist > 0) {
            currentDistance = dist;
        }
      }
    }
  }
}

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000);
  Serial.println(F("\n====== LORA MESH REPEATER NODE ======"));
  Serial.print(F("Node ID Assigned: "));
  Serial.println(MY_NODE_ID);

  // 1. Initialize Sensor
#if MOCK_SENSOR_MODE
  Serial.println(F("MOCK MODE: Simulating ultrasonic sensor hardware."));
  currentDistance = 1500; // Seed starting value
#else
  SENSOR_SERIAL.begin(9600);
  Serial.print(F("Warming up ultrasonic sensor... "));
  while (currentDistance == 0) {
    updateSensorReading();
    delay(5);
  }
  Serial.println(F("Locked! No 0mm blind spots."));
#endif

  // 2. Initialize RadioHead Radio Engine
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  Serial.print(F("Resetting SPI LoRa Chip... "));
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  if (!rf95.init()) {
    Serial.println(F("FAILED! Check SPI Pins."));
    while (1); 
  }
  Serial.println(F("OK!"));

  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println(F("Frequency set failed"));
    while (1);
  }
  
  rf95.setTxPower(23, false);

  // MESH CONFIGURATION:
  // Set our Node ID in the hardware
  rf95.setThisAddress(MY_NODE_ID);
  rf95.setHeaderFrom(MY_NODE_ID);
  
  // CRITICAL: Promiscuous mode allows the radio to hear packets that are NOT 
  // destined for us! Without this, the hardware physically rejects packets 
  // meant for others, breaking the repeater functionality!
  rf95.setPromiscuous(true); 
}

// -----------------------------------------------------------------------------
// Loop
// -----------------------------------------------------------------------------
void loop() {
#if MOCK_SENSOR_MODE
  // Automatically generate a highly realistic fluctuating water level distance
  currentDistance = random(1200, 2000); 
#else
  updateSensorReading();
#endif

  // =========================================================================
  // MESH RELAY / RECEIVER LOGIC
  // =========================================================================
  if (rf95.available()) {
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);

    if (rf95.recv(buf, &len)) {
      // Deconstruct the incoming packet headers
      uint8_t to = rf95.headerTo();
      uint8_t from = rf95.headerFrom();
      uint8_t id = rf95.headerId();
      uint8_t flags = rf95.headerFlags();

      // MESH DEDUPLICATION CHECK: Have we seen this exact packet sequence recently?
      if (seen_messages[from] != id) {
        seen_messages[from] = id; // Store it so we never relay it again

        // If this packet is NOT meant for us, and NOT a broadcast (255), we must RELAY IT!
        if (to != 255 && to != MY_NODE_ID) {
          Serial.print(F("[REPEATER] Relaying a packet originating from Node "));
          Serial.print(from);
          Serial.print(F(" intended for Node "));
          Serial.println(to);

          // Force the radio headers to precisely impersonate the original sender
          rf95.setHeaderTo(to);
          rf95.setHeaderFrom(from);
          rf95.setHeaderId(id);
          rf95.setHeaderFlags(flags);
          
          // Re-transmit the payload verbatim
          rf95.send(buf, len);
          rf95.waitPacketSent();
          
          // Reset headers back to our true identity after finishing
          rf95.setHeaderFrom(MY_NODE_ID);
        }
      }
    }
  }

  // =========================================================================
  // PERIODIC SENSOR BROADCASTING LOGIC
  // =========================================================================
  if (millis() - lastTransmitTime > TX_INTERVAL_MS) {
    lastTransmitTime = millis();
    
    // Increment our personal outgoing sequence ID (starts at 1!)
    my_message_sequence++; 
    
    char radiopacket[32];
    snprintf(radiopacket, sizeof(radiopacket), "Distance:%dmm", currentDistance);
    
    Serial.print(F("[TX] Broadcasting to Gateway (Node "));
    Serial.print(GATEWAY_ID);
    Serial.print(F(") seq: "));
    Serial.print(my_message_sequence);
    Serial.print(F(" -> "));
    Serial.println(radiopacket);

    // Apply target routing headers for our own transmission
    rf95.setHeaderTo(GATEWAY_ID);
    rf95.setHeaderFrom(MY_NODE_ID);
    rf95.setHeaderId(my_message_sequence);
    rf95.setHeaderFlags(0);
    
    // Send standard string over the air
    rf95.send((uint8_t *)radiopacket, strlen(radiopacket));
    rf95.waitPacketSent();
    
    Serial.println(F("[TX] Packet launched!"));
    Serial.println(F("-------------------------------------------------"));
  }
}
