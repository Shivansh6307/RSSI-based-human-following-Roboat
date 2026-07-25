/*
  RSSI HUMAN-FOLLOWING ROBOT — MASTER NODE
  ===========================================
  Flash this to the 5th ESP32 (the "brain") mounted centrally on the robot.
  It receives smoothed RSSI values from all 4 receiver nodes via ESP-NOW,
  decides which direction the person is, and drives the motors via an
  L298N motor driver.

  ASSUMPTION: 2-wheel differential drive (2 DC motors) via L298N.
  If you're using a different motor driver (TB6612, BTS7960, etc.) or
  4-wheel drive, only the motor control section (bottom) needs changing —
  the RSSI logic stays the same.

  Wiring (L298N -> ESP32), adjust pins if yours differ:
    ENA -> GPIO 14   (left motor speed, PWM)
    IN1 -> GPIO 27   (left motor dir 1)
    IN2 -> GPIO 26   (left motor dir 2)
    ENB -> GPIO 25   (right motor speed, PWM)
    IN3 -> GPIO 33   (right motor dir 1)
    IN4 -> GPIO 32   (right motor dir 2)
*/

#include <esp_now.h>
#include <WiFi.h>

// ------------- CONFIG -------------
const int ENA = 14, IN1 = 27, IN2 = 26;   // left motor
const int ENB = 25, IN3 = 33, IN4 = 32;   // right motor

const float TURN_THRESHOLD_DB = 4.0;   // dBm difference needed to trigger a turn (tune this)
const float STOP_RSSI_DBM = -40.0;     // closer than this (less negative) = stop, person is close enough
const float OUT_OF_RANGE_DBM = -85.0;  // weaker than this on ALL nodes = person too far / lost signal

const int NORMAL_SPEED = 180;   // PWM 0-255
const int TURN_SPEED = 160;

const unsigned long DATA_TIMEOUT_MS = 2000; // if a node stops reporting, treat as stale
// -----------------------------------

typedef struct {
  uint8_t node_id;
  float rssi;
} rssi_packet_t;

// node_id mapping: 0=FL, 1=FR, 2=BL, 3=BR
float nodeRSSI[4] = {-100, -100, -100, -100};
unsigned long lastSeen[4] = {0, 0, 0, 0};

void onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  rssi_packet_t packet;
  memcpy(&packet, incomingData, sizeof(packet));
  if (packet.node_id < 4) {
    nodeRSSI[packet.node_id] = packet.rssi;
    lastSeen[packet.node_id] = millis();
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(ENA, OUTPUT); pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  stopMotors();

  WiFi.mode(WIFI_STA);
  Serial.print("Master MAC (for reference): ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    return;
  }
  esp_now_register_recv_cb(onDataRecv);
  Serial.println("Master ready. Waiting for receiver data...");
}

void loop() {
  unsigned long now = millis();

  // Check for stale nodes (not reporting = probably out of WiFi range or crashed)
  bool anyStale = false;
  for (int i = 0; i < 4; i++) {
    if (now - lastSeen[i] > DATA_TIMEOUT_MS) anyStale = true;
  }

  float FL = nodeRSSI[0], FR = nodeRSSI[1], BL = nodeRSSI[2], BR = nodeRSSI[3];

  float frontAvg = (FL + FR) / 2.0;
  float backAvg  = (BL + BR) / 2.0;
  float leftAvg  = (FL + BL) / 2.0;
  float rightAvg = (FR + BR) / 2.0;
  float overallAvg = (FL + FR + BL + BR) / 4.0;

  Serial.print("FL:"); Serial.print(FL);
  Serial.print(" FR:"); Serial.print(FR);
  Serial.print(" BL:"); Serial.print(BL);
  Serial.print(" BR:"); Serial.print(BR);
  Serial.print(" | front:"); Serial.print(frontAvg);
  Serial.print(" back:"); Serial.print(backAvg);
  Serial.print(" left:"); Serial.print(leftAvg);
  Serial.print(" right:"); Serial.println(rightAvg);

  if (overallAvg < OUT_OF_RANGE_DBM || anyStale) {
    // Signal lost — stop and wait (safer than guessing direction on bad data)
    stopMotors();
    Serial.println("-> Signal weak/lost. Stopped.");
  }
  else if (overallAvg > STOP_RSSI_DBM) {
    // Person is close enough — stop
    stopMotors();
    Serial.println("-> Close enough. Stopped.");
  }
  else if (frontAvg < backAvg - TURN_THRESHOLD_DB) {
    // Person is behind — turn around (in place)
    turnRight(TURN_SPEED);
    Serial.println("-> Person behind. Turning to reorient.");
  }
  else if (leftAvg > rightAvg + TURN_THRESHOLD_DB) {
    turnLeft(TURN_SPEED);
    Serial.println("-> Turning left.");
  }
  else if (rightAvg > leftAvg + TURN_THRESHOLD_DB) {
    turnRight(TURN_SPEED);
    Serial.println("-> Turning right.");
  }
  else {
    moveForward(NORMAL_SPEED);
    Serial.println("-> Moving forward.");
  }

  delay(200); // decision loop rate
}

// ---------------- Motor control (L298N, 2-wheel differential drive) ----------------

void moveForward(int speed) {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  analogWrite(ENA, speed);
  analogWrite(ENB, speed);
}

void turnLeft(int speed) {
  // left motor backward, right motor forward = turn in place
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  analogWrite(ENA, speed);
  analogWrite(ENB, speed);
}

void turnRight(int speed) {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);
  analogWrite(ENA, speed);
  analogWrite(ENB, speed);
}

void stopMotors() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
}
