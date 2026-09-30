/*
  ================================================================
  FULL AVIONICS REFERENCE SYSTEM
  Akashyan Spardha 2026 — Solid Fuel Rocketry Challenge
  Team: Pratap (Team Code: AYS26-D5FTA)
  ================================================================

  Required libraries:
    - "Adafruit BMP280" (+ "Adafruit Unified Sensor")
    - "ESP32Servo"
    - "LoRa" by Sandeep Mistry

  Pinout (matches the official spec — only use these if this runs
  on its own separate board, not the competition-issued ESP32):
    BMP280  SDA -> GPIO21      BMP280  SCL -> GPIO22
    RA-02   MOSI -> GPIO23     RA-02   MISO -> GPIO19
    RA-02   SCK -> GPIO18      RA-02   NSS  -> GPIO5
    RA-02   RESET -> GPIO14    RA-02   DIO0 -> GPIO26
    Servo   Signal -> GPIO27   (own separate battery)
*/

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_BMP280.h>
#include <ESP32Servo.h>
#include <LoRa.h>

// ================== Pin definitions ==================
#define BMP_SDA      21
#define BMP_SCL      22

#define LORA_SCK     18
#define LORA_MISO    19
#define LORA_MOSI    23
#define LORA_NSS     5
#define LORA_RST     14
#define LORA_DIO0    26

#define SERVO_PIN    27

#define LORA_FREQUENCY 434.5E6   // per competition spec — confirm before dual-TX use

// ================== Servo angles ==================
#define SERVO_LOCKED_ANGLE   0
#define SERVO_FIRE_ANGLE     90

// ================== Detection tuning ==================
#define LAUNCH_DETECT_ALT       100.0
#define SEA_LEVEL_HPA           1013.25
#define MAX_PLAUSIBLE_STEP      100.0
#define CANDIDATE_TOLERANCE     5.0
#define CANDIDATE_CONFIRM_COUNT 3
#define DROP_CONFIRM_READINGS   3

// ================== Team / identification ==================
// Team code is for YOUR reference only — printed to Serial for
// identification during testing. It is NOT transmitted in the
// packet (the packet field is 1 numeric byte, see note above).
#define TEAM_CODE "AYS26-D5FTA"
#define TEAM_NAME "Pratap"

// ================== Telemetry packet format ==================
// Matches Annexure II Section 10:
//   Rocket ID  (1 byte)   <-- numeric, organizer-assigned at inspection
//   TYPE       (1 byte)  0x01 = altitude, 0x02 = apogee
//   SEQ        (2 bytes) 16-bit sequence number
//   ALTITUDE   (2 bytes) unsigned, 0.1 m units
#define ROCKET_ID 0   // placeholder — replace with organizer-assigned NUMERIC ID at inspection
uint16_t seqNumber = 0;

const unsigned long TELEMETRY_INTERVAL_MS = 500; // 2 packets/sec per spec
unsigned long lastTelemetrySend = 0;

// ================== State ==================
Adafruit_BMP280 bmp;
Servo ejectionServo;

float groundAltitude   = 0;
float lastValidAlt     = 0;
float confirmedMaxAlt  = 0;
float pendingCandidate = 0;
int   pendingCount     = 0;
int   dropCount        = 0;

bool launched   = false;
bool ejected    = false;
bool apogeeSent = false;
bool firstReading = true;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== Full Avionics Reference System ===");
  Serial.print("Team: "); Serial.print(TEAM_NAME);
  Serial.print(" | Code: "); Serial.println(TEAM_CODE);
  Serial.println("(Rocket ID for packets is numeric — set after organizer assigns it)");

  // ---------- BMP280 ----------
  Wire.begin(BMP_SDA, BMP_SCL);
  if (!bmp.begin(0x76) && !bmp.begin(0x77)) {
    Serial.println("BMP280 NOT DETECTED. Check wiring.");
    while (1) delay(1000);
  }
  Serial.println("BMP280 OK.");
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                   Adafruit_BMP280::SAMPLING_X2,
                   Adafruit_BMP280::SAMPLING_X16,
                   Adafruit_BMP280::FILTER_X16,
                   Adafruit_BMP280::STANDBY_MS_63);

  // ---------- Servo ----------
  ejectionServo.attach(SERVO_PIN);
  ejectionServo.write(SERVO_LOCKED_ANGLE);
  Serial.println("Servo initialized — locked position.");

  // ---------- LoRa ----------
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  LoRa.setPins(LORA_NSS, LORA_RST, LORA_DIO0);
  if (LoRa.begin(LORA_FREQUENCY)) {
    Serial.println("LoRa OK.");
  } else {
    Serial.println("LoRa FAILED — check wiring/antenna.");
  }

  // ---------- Ground reference ----------
  delay(2000);
  groundAltitude  = bmp.readAltitude(SEA_LEVEL_HPA);
  lastValidAlt    = groundAltitude;
  confirmedMaxAlt = groundAltitude;
  Serial.print("Ground altitude reference: ");
  Serial.print(groundAltitude);
  Serial.println(" m");

  Serial.println("System armed. Waiting for launch...");
}

void loop() {
  float rawAltitude = bmp.readAltitude(SEA_LEVEL_HPA);

  // ---------- Glitch rejection ----------
  if (!firstReading && fabs(rawAltitude - lastValidAlt) > MAX_PLAUSIBLE_STEP) {
    Serial.println("Glitch rejected — implausible jump, ignored.");
    delay(100);
    return;
  }
  firstReading = false;

  float altAboveGround = rawAltitude - groundAltitude;

  // ---------- Launch detection ----------
  if (!launched && altAboveGround > LAUNCH_DETECT_ALT) {
    launched = true;
    Serial.println(">>> LAUNCH DETECTED (above 100 m) <<<");
  }

  if (launched && !ejected) {
    // ---------- Candidate-confirmed max tracking ----------
    if (rawAltitude > confirmedMaxAlt) {
      if (pendingCount == 0 || fabs(rawAltitude - pendingCandidate) <= CANDIDATE_TOLERANCE) {
        pendingCandidate = rawAltitude;
        pendingCount++;
        if (pendingCount >= CANDIDATE_CONFIRM_COUNT) {
          confirmedMaxAlt = pendingCandidate;
          pendingCount = 0;
          Serial.print("New confirmed max: ");
          Serial.print(confirmedMaxAlt - groundAltitude);
          Serial.println(" m");
        }
      } else {
        pendingCandidate = rawAltitude;
        pendingCount = 1;
      }
      dropCount = 0;
    } else {
      dropCount++;
    }

    Serial.print("Alt: "); Serial.print(altAboveGround);
    Serial.print(" m | ConfirmedMax: "); Serial.print(confirmedMaxAlt - groundAltitude);
    Serial.print(" m | DropCount: "); Serial.println(dropCount);

    // ---------- Fire ejection ----------
    if (dropCount >= DROP_CONFIRM_READINGS) {
      Serial.println(">>> APOGEE CONFIRMED — FIRING EJECTION <<<");
      fireServo();
      apogeeSent = false; // trigger repeated apogee packets below
    }
  }

  // ---------- Telemetry transmission (own backup link) ----------
  if (millis() - lastTelemetrySend > TELEMETRY_INTERVAL_MS) {
    lastTelemetrySend = millis();
    if (ejected) {
      sendPacket(0x02, confirmedMaxAlt - groundAltitude); // apogee packet, repeated
    } else {
      sendPacket(0x01, altAboveGround); // normal altitude packet
    }
  }

  lastValidAlt = rawAltitude;
  delay(100);
}

void fireServo() {
  ejectionServo.write(SERVO_FIRE_ANGLE);
  delay(1000);
  ejectionServo.write(SERVO_LOCKED_ANGLE);
  ejected = true;
  Serial.println("Ejection complete.");
}

// Builds and sends a 6-byte packet: ID, TYPE, SEQ(2), ALTITUDE(2)
void sendPacket(uint8_t type, float altitudeMeters) {
  uint16_t altValue = (uint16_t)(altitudeMeters * 10.0); // 0.1 m units
  seqNumber++;

  uint8_t packet[6];
  packet[0] = ROCKET_ID;
  packet[1] = type;
  packet[2] = (seqNumber >> 8) & 0xFF;
  packet[3] = seqNumber & 0xFF;
  packet[4] = (altValue >> 8) & 0xFF;
  packet[5] = altValue & 0xFF;

  LoRa.beginPacket();
  LoRa.write(packet, 6);
  LoRa.endPacket();

  Serial.print("Sent packet | Type: 0x");
  Serial.print(type, HEX);
  Serial.print(" | Seq: ");
  Serial.print(seqNumber);
  Serial.print(" | Alt: ");
  Serial.println(altitudeMeters);
}
