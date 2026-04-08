#include <Arduino.h>
#include <SPI.h>
#include <lmic.h>
#include <hal/hal.h>

// -----------------------------------------------------------------------------
// TTN (The Things Network) Configuration - OTAA
// -----------------------------------------------------------------------------

// APPEUI: Must be in little-endian format (LSB first). Reversing bytes if
// copied from TTN console.
static const u1_t PROGMEM APPEUI[8] = {0x70, 0x73, 0x34, 0x29,
                                       0x70, 0x86, 0x95, 0x43};
void os_getArtEui(u1_t *buf) { memcpy_P(buf, APPEUI, 8); }

// DEVEUI: Must be in little-endian format (LSB first). Reversing bytes if
// copied from TTN console.
static const u1_t PROGMEM DEVEUI[8] = {0x39, 0x66, 0x07, 0xD0,
                                       0x7E, 0xD5, 0xB3, 0x70};
void os_getDevEui(u1_t *buf) { memcpy_P(buf, DEVEUI, 8); }

// APPKEY: Must be in big-endian format (MSB first). Copy as-is from TTN
// console.
static const u1_t PROGMEM APPKEY[16] = {0xF9, 0xB2, 0xAC, 0xFD, 0xDC, 0x4F,
                                        0x21, 0x89, 0xFD, 0x21, 0xF8, 0x4D,
                                        0x0F, 0x0C, 0xEB, 0x66};
void os_getDevKey(u1_t *buf) { memcpy_P(buf, APPKEY, 16); }

// -----------------------------------------------------------------------------
// Payload Data and Timing
// -----------------------------------------------------------------------------
static uint8_t mydata[] = "Hello";
static osjob_t sendjob;

// Schedule TX every this many seconds (might become longer due to duty cycle
// limitations).
const unsigned TX_INTERVAL = 60;

// -----------------------------------------------------------------------------
// Pin Mapping for Adafruit Feather M0 LoRa
// -----------------------------------------------------------------------------
// IMPORTANT HARDWARE NOTE:
// You MUST solder a small wire connecting the LoRa module's "dio1" pin to the
// Feather's "6" pin. The LMIC library requires DIO0, DIO1 connected to unique
// GPIO pins to function correctly!
const lmic_pinmap lmic_pins = {
    .nss = 8,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 4,
    .dio = {3, 6, LMIC_UNUSED_PIN}, // DIO0 = pin 3, DIO1 = pin 6 (external
                                    // jumper required)
};

void do_send(osjob_t *j) {
  // Check if there is not a current TX/RX job running
  if (LMIC.opmode & OP_TXRXPEND) {
    Serial.println(F("OP_TXRXPEND, not sending"));
  } else {
    // Prepare upstream data transmission at the next possible time.
    LMIC_setTxData2(1, mydata, sizeof(mydata) - 1,
                    0); // -1 removes the null terminator from the string
    Serial.println(F("Packet queued for transmission"));
  }
}

void onEvent(ev_t ev) {
  Serial.print(os_getTime());
  Serial.print(": ");
  switch (ev) {
  case EV_SCAN_TIMEOUT:
    Serial.println(F("EV_SCAN_TIMEOUT"));
    break;
  case EV_BEACON_FOUND:
    Serial.println(F("EV_BEACON_FOUND"));
    break;
  case EV_BEACON_MISSED:
    Serial.println(F("EV_BEACON_MISSED"));
    break;
  case EV_BEACON_TRACKED:
    Serial.println(F("EV_BEACON_TRACKED"));
    break;
  case EV_JOINING:
    Serial.println(F("EV_JOINING"));
    break;
  case EV_JOINED:
    Serial.println(F("EV_JOINED"));
    // Disable link check validation (automatically enabled
    // during join, but not supported by TTN at this time).
    LMIC_setLinkCheckMode(0);
    break;
  case EV_RFU1:
    Serial.println(F("EV_RFU1"));
    break;
  case EV_JOIN_FAILED:
    Serial.println(F("EV_JOIN_FAILED"));
    break;
  case EV_REJOIN_FAILED:
    Serial.println(F("EV_REJOIN_FAILED"));
    break;
  case EV_TXCOMPLETE:
    Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
    if (LMIC.txrxFlags & TXRX_ACK)
      Serial.println(F("Received ack"));
    if (LMIC.dataLen) {
      Serial.print(F("Received "));
      Serial.print(LMIC.dataLen);
      Serial.println(F(" bytes of payload"));
    }
    // Schedule next transmission
    os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(TX_INTERVAL),
                        do_send);
    break;
  case EV_LOST_TSYNC:
    Serial.println(F("EV_LOST_TSYNC"));
    break;
  case EV_RESET:
    Serial.println(F("EV_RESET"));
    break;
  case EV_RXCOMPLETE:
    // data received in ping slot
    Serial.println(F("EV_RXCOMPLETE"));
    break;
  case EV_LINK_DEAD:
    Serial.println(F("EV_LINK_DEAD"));
    break;
  case EV_LINK_ALIVE:
    Serial.println(F("EV_LINK_ALIVE"));
    break;
  case EV_TXSTART:
    Serial.println(F("EV_TXSTART"));
    break;
  case EV_TXCANCELED:
    Serial.println(F("EV_TXCANCELED"));
    break;
  case EV_RXSTART:
    // do not print anything -- it wrecks timing
    break;
  case EV_JOIN_TXCOMPLETE:
    Serial.println(F("EV_JOIN_TXCOMPLETE: no JoinAccept"));
    break;
  default:
    Serial.print(F("Unknown event: "));
    Serial.println((unsigned)ev);
    break;
  }
}

void setup() {
  Serial.begin(9600);
  // Wait up to 5 seconds for serial connection to be established
  while (!Serial && millis() < 5000)
    ;
  Serial.println(F("Starting LoRaWAN node..."));

  // Ensure LoRa SPI is ready (useful on some boards)
  pinMode(8, OUTPUT);
  digitalWrite(8, HIGH);

  // LMIC initialize
  os_init();

  // Reset the MAC state. Session and pending data transfers will be discarded.
  LMIC_reset();

  // Allow for up to 1% clock error margin to accommodate inaccurate oscillators
  LMIC_setClockError(MAX_CLOCK_ERROR * 1 / 100);

  // Initial SubBand selection for US915. Most US TTN gateways use sub-band 2
  // (channels 8-15)
  LMIC_selectSubBand(1); // 1 = sub-band 2 in LMIC

  // Start job (sending automatically starts OTAA too)
  do_send(&sendjob);
}

void loop() {
  // Run the LMIC operating system loop to process jobs and radio events
  os_runloop_once();
}