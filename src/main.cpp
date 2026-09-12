// =============================================================================
// ESP32 Hexapod — Full 18-Servo Dual-PCA9685 Controller
// PlatformIO | Framework: Arduino | Board: ESP32 DevKit (30-pin)
//
// Hardware Wiring (both PCA9685 boards daisy-chained on same I²C bus):
//   ESP32 SDA (GPIO 21) ─── PCA9685 SDA (both boards)
//   ESP32 SCL (GPIO 22) ─── PCA9685 SCL (both boards)
//   ESP32 3V3           ─── PCA9685 VCC (Logic power, both boards)
//   ESP32 GND           ─── PCA9685 GND (Common ground)
//   ZX-052 6V OUT       ─── PCA9685 V+  (Servo power, both boards)
//
// Channel Map:
//   Board 0x40 (Main): Leg1(D=ch0,M=ch1,L=ch2) Leg2(D=ch3,M=ch4,L=ch5) Leg3(D=ch6,M=ch7,L=ch8)
//   Board 0x43 (Aux):  Leg6(D=ch0,M=ch1,L=ch2) Leg5(D=ch3,M=ch4,L=ch5) Leg4(D=ch6,M=ch7,L=ch8)
//
// Leg Layout (top view, front facing up):
//                ▲ FRONT (Forward Gait)
//          Leg 4 ╲       ╱ Leg 1 (Front Right)
//                 ╲ ─── ╱
//   (Mid Left) Leg 5 │   │ Leg 2 (Mid Right)
//                 ╱ ─── ╲
//          Leg 6 ╱       ╲ Leg 3 (Rear Right)
//                ▼ REAR
//
// Joint Degrees of Freedom per Leg (Side View):
//   Chassis ──[L: Coxa (Yaw)]──┬──[M: Femur (Pitch)]──┬──[D: Tibia (Pitch)]── Foot
//                              │                      │
//                    (Horizontal Swing)       (Vertical Lift)      (Ground Contact)
//
// Tripod Groups:
//   Group A: Legs 1, 3, 5  |  Group B: Legs 2, 4, 6
// =============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include "BluetoothSerial.h"

// ---------------------------------------------------------------------------
// Hardware Configuration
// ---------------------------------------------------------------------------
#define I2C_SDA         21
#define I2C_SCL         22
#define SERVO_FREQ      50          // Standard 50 Hz for MG996R servos
#define OSC_FREQ        27000000    // PCA9685 internal oscillator
#define PULSE_MIN       500         // ~0°  pulse width (us)
#define PULSE_MID       1500        // ~90° pulse width (us)
#define PULSE_MAX       2500        // ~180° pulse width (us)
#define PCA_MAIN_ADDR   0x40        // Board 0x40 (default) → Right Side (Legs 1, 2, 3)
#define PCA_AUX_ADDR    0x43        // Board 0x43 (A0+A1)   → Left Side  (Legs 4, 5, 6)
#define NUM_LEGS        6
#define UPDATE_INTERVAL 20          // 20 ms = 50 Hz update rate

// ---------------------------------------------------------------------------
// PCA9685 Driver Instances
// ---------------------------------------------------------------------------
Adafruit_PWMServoDriver pcaMain(PCA_MAIN_ADDR);
Adafruit_PWMServoDriver pcaAux(PCA_AUX_ADDR);
bool boardMainOK = false;
bool boardAuxOK  = false;

// ---------------------------------------------------------------------------
// Bluetooth Serial
// ---------------------------------------------------------------------------
BluetoothSerial SerialBT;
static const char* BT_NAME = "HEXA-SPIDER";

// ---------------------------------------------------------------------------
// Data Structures
// ---------------------------------------------------------------------------
struct Joint {
  Adafruit_PWMServoDriver* board;   // Pointer to pcaMain or pcaAux
  uint8_t  ch;                      // PCA9685 channel (0–15)
  float    target;                  // Desired angle before trim (degrees)
  float    angle;                   // Actual commanded angle after trim+clamp
  float    trim;                    // Software calibration trim (degrees)
  float    minA;                    // Safe minimum angle
  float    maxA;                    // Safe maximum angle
  bool     inv;                     // Invert servo direction (180 - angle)
  bool     active;                  // true if the board was detected
};

struct Leg {
  uint8_t     num;                  // Leg number 1–6
  const char* name;                 // Human-readable name
  bool        leftSide;             // true for legs 4, 5, 6
  Joint       L;                    // Coxa  (hip lateral swing)
  Joint       M;                    // Femur (mid thigh lift)
  Joint       D;                    // Tibia (shin / foot)
};

Leg legs[NUM_LEGS];

// ---------------------------------------------------------------------------
// Motion Modes
// ---------------------------------------------------------------------------
enum Mode { MODE_HOLD, MODE_WALK, MODE_DANCE, MODE_WAVE, MODE_SWEEP };
Mode mode = MODE_HOLD;
unsigned long lastTickMs = 0;

// Walk tuning parameters
uint32_t walkPeriod = 2000;         // Full gait cycle duration (ms)
float    walkSwing  = 25.0f;        // Coxa swing range (degrees from center)
float    walkLift   = 30.0f;        // Femur lift height (degrees)

// Sweep state
float sweepPos = 90.0f;
float sweepDir = 1.0f;
float sweepRate = 60.0f;            // degrees per second

// ---------------------------------------------------------------------------
// Forward Declarations
// ---------------------------------------------------------------------------
void initJoint(Joint& j, Adafruit_PWMServoDriver* board, uint8_t ch,
               float minA, float maxA);
void initLegs();
void scanI2C();
void initBoard(Adafruit_PWMServoDriver& pca, const char* label, bool detected);
void writeJoint(Joint& j, float targetDeg);
void setLeg(int idx, float lDeg, float mDeg, float dDeg);
void centerAll();
void standAll();
void relaxAll();
void updateMotion();
void updateWalk(unsigned long now);
void updateDance(unsigned long now);
void updateWave(unsigned long now);
void updateSweep(float dt);
void processCmd(String cmd);
void printHelp();
void printStatus();

// ===========================================================================
//  SETUP
// ===========================================================================
void setup() {
  Serial.begin(115200);
  delay(800);

  Serial.println(F("\n=========================================================="));
  Serial.println(F("  ESP32 Hexapod  |  18-Servo Dual-PCA9685 Controller"));
  Serial.println(F("=========================================================="));

  // ---- I²C Bus ----
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);            // 400 kHz Fast-mode I²C
  Serial.println(F("[I2C] Bus started: SDA=21, SCL=22, 400 kHz"));

  // ---- Detect PCA9685 boards ----
  scanI2C();

  // ---- Build leg/channel map ----
  initLegs();

  // ---- Initialise detected boards ----
  if (boardMainOK) {
    pcaMain.begin();
    pcaMain.setPWMFreq(SERVO_FREQ);
    delay(10);
    Serial.println(F("[OK] PCA9685 (0x40) ready  -> Legs 1, 2, 3 (Right)"));
  }
  if (boardAuxOK) {
    pcaAux.begin();
    pcaAux.setPWMFreq(SERVO_FREQ);
    delay(10);
    Serial.println(F("[OK] PCA9685 (0x43) ready  -> Legs 4, 5, 6 (Left)"));
  }

  // ---- Propagate detection status to every joint ----
  for (int i = 0; i < NUM_LEGS; i++) {
    bool ok = (legs[i].L.board == &pcaMain) ? boardMainOK : boardAuxOK;
    legs[i].L.active = ok;
    legs[i].M.active = ok;
    legs[i].D.active = ok;
  }

  // ---- Bluetooth ----
  if (SerialBT.begin(BT_NAME)) {
    Serial.printf("[OK] Bluetooth active: \"%s\"\n", BT_NAME);
  } else {
    Serial.println(F("[WARN] Bluetooth init failed"));
  }

  // ---- Safe startup: center every servo to 90° ----
  centerAll();
  // Guarantee every channel 0-15 on detected boards receives 90° (1500us / 307 ticks):
  if (boardMainOK) {
    for (uint8_t ch = 0; ch < 16; ch++) pcaMain.setPWM(ch, 0, 307);
  }
  if (boardAuxOK) {
    for (uint8_t ch = 0; ch < 16; ch++) pcaAux.setPWM(ch, 0, 307);
  }
  Serial.println(F("[BOOT] All servos centered to 90 deg (1500 us / 307 ticks) — ready for commands."));
  Serial.println();
  printHelp();
}

// ===========================================================================
//  MAIN LOOP  (non-blocking)
// ===========================================================================
void loop() {
  // USB Serial commands
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() > 0) processCmd(cmd);
  }

  // Bluetooth commands
  if (SerialBT.available()) {
    String cmd = SerialBT.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() > 0) processCmd(cmd);
  }

  // Continuous motion update (50 Hz, non-blocking)
  updateMotion();
}

// ===========================================================================
//  LEG / JOINT INITIALISATION
// ===========================================================================

void initJoint(Joint& j, Adafruit_PWMServoDriver* board, uint8_t ch,
               float minA, float maxA) {
  j.board  = board;
  j.ch     = ch;
  j.target = 90.0f;
  j.angle  = 90.0f;
  j.trim   = 0.0f;
  j.minA   = minA;
  j.maxA   = maxA;
  j.inv    = false;
  j.active = false;   // Updated after I²C scan
}

void initLegs() {
  // ===== Board 0x40 (Main) — RIGHT SIDE: Legs 1, 2, 3 =====

  // Leg 1 — Front Right:  D1=ch0, M1=ch1, L1=ch2
  legs[0].num = 1;  legs[0].name = "Front Right";  legs[0].leftSide = false;
  initJoint(legs[0].L, &pcaMain, 2,  30.0f, 150.0f);   // Coxa
  initJoint(legs[0].M, &pcaMain, 1,  20.0f, 160.0f);   // Femur
  initJoint(legs[0].D, &pcaMain, 0,  20.0f, 160.0f);   // Tibia

  // Leg 2 — Mid Right:    D2=ch3, M2=ch4, L2=ch5
  legs[1].num = 2;  legs[1].name = "Mid Right";    legs[1].leftSide = false;
  initJoint(legs[1].L, &pcaMain, 5,  30.0f, 150.0f);
  initJoint(legs[1].M, &pcaMain, 4,  20.0f, 160.0f);
  initJoint(legs[1].D, &pcaMain, 3,  20.0f, 160.0f);

  // Leg 3 — Rear Right:   D3=ch6, M3=ch7, L3=ch8
  legs[2].num = 3;  legs[2].name = "Rear Right";   legs[2].leftSide = false;
  initJoint(legs[2].L, &pcaMain, 8,  30.0f, 150.0f);
  initJoint(legs[2].M, &pcaMain, 7,  20.0f, 160.0f);
  initJoint(legs[2].D, &pcaMain, 6,  20.0f, 160.0f);

  // ===== Board 0x43 (Aux) — LEFT SIDE: Legs 4, 5, 6 =====

  // Leg 4 — Front Left:   D4=ch6, M4=ch7, L4=ch8
  legs[3].num = 4;  legs[3].name = "Front Left";   legs[3].leftSide = true;
  initJoint(legs[3].L, &pcaAux, 8,  30.0f, 150.0f);
  initJoint(legs[3].M, &pcaAux, 7,  20.0f, 160.0f);
  initJoint(legs[3].D, &pcaAux, 6,  20.0f, 160.0f);

  // Leg 5 — Mid Left:     D5=ch3, M5=ch4, L5=ch5
  legs[4].num = 5;  legs[4].name = "Mid Left";     legs[4].leftSide = true;
  initJoint(legs[4].L, &pcaAux, 5,  30.0f, 150.0f);
  initJoint(legs[4].M, &pcaAux, 4,  20.0f, 160.0f);
  initJoint(legs[4].D, &pcaAux, 3,  20.0f, 160.0f);

  // Leg 6 — Rear Left:    D6=ch0, M6=ch1, L6=ch2
  legs[5].num = 6;  legs[5].name = "Rear Left";    legs[5].leftSide = true;
  initJoint(legs[5].L, &pcaAux, 2,  30.0f, 150.0f);
  initJoint(legs[5].M, &pcaAux, 1,  20.0f, 160.0f);
  initJoint(legs[5].D, &pcaAux, 0,  20.0f, 160.0f);
}

// ===========================================================================
//  SERVO CONTROL
// ===========================================================================

// Write a target angle to a single joint (applies trim + clamping + inversion)
void writeJoint(Joint& j, float targetDeg) {
  j.target = targetDeg;
  float cmd = targetDeg + j.trim;
  cmd = constrain(cmd, j.minA, j.maxA);
  if (j.inv) {
    cmd = 180.0f - cmd;
  }
  j.angle = cmd;

  // Convert degrees (0..180) -> microseconds (500..2500 us)
  uint16_t us = (uint16_t)map((long)(cmd * 10.0f), 0L, 1800L,
                              (long)PULSE_MIN, (long)PULSE_MAX);
  us = constrain(us, (uint16_t)PULSE_MIN, (uint16_t)PULSE_MAX);

  // Convert microseconds to PCA9685 12-bit tick (50Hz = 20,000 us across 4096 counts)
  // tick = us * 4096 / 20000 = us * 0.2048
  uint16_t tick = (uint16_t)((float)us * 4096.0f / 20000.0f + 0.5f);
  tick = constrain(tick, (uint16_t)100, (uint16_t)550);

  if (j.board != nullptr) {
    j.board->setPWM(j.ch, 0, tick);
  }
}

// Set all 3 joints of one leg (by index 0–5)
void setLeg(int idx, float lDeg, float mDeg, float dDeg) {
  if (idx < 0 || idx >= NUM_LEGS) return;
  writeJoint(legs[idx].L, lDeg);
  writeJoint(legs[idx].M, mDeg);
  writeJoint(legs[idx].D, dDeg);
}

// Center all 18 servos to 90°
void centerAll() {
  mode = MODE_HOLD;
  for (int i = 0; i < NUM_LEGS; i++) setLeg(i, 90.0f, 90.0f, 90.0f);
}

// Stand pose: legs angled down for support
void standAll() {
  mode = MODE_HOLD;
  for (int i = 0; i < NUM_LEGS; i++) setLeg(i, 90.0f, 60.0f, 120.0f);
}

// Relax: disable PWM on all channels (servos go limp)
void relaxAll() {
  mode = MODE_HOLD;
  for (int i = 0; i < NUM_LEGS; i++) {
    if (legs[i].L.active) legs[i].L.board->setPWM(legs[i].L.ch, 0, 4096);
    if (legs[i].M.active) legs[i].M.board->setPWM(legs[i].M.ch, 0, 4096);
    if (legs[i].D.active) legs[i].D.board->setPWM(legs[i].D.ch, 0, 4096);
  }
}

// ===========================================================================
//  MOTION UPDATE DISPATCHER  (50 Hz, non-blocking)
// ===========================================================================
void updateMotion() {
  unsigned long now = millis();
  if (now - lastTickMs < UPDATE_INTERVAL) return;
  float dt = (float)(now - lastTickMs) / 1000.0f;
  lastTickMs = now;

  switch (mode) {
    case MODE_HOLD:  break;                 // Nothing to do
    case MODE_WALK:  updateWalk(now);  break;
    case MODE_DANCE: updateDance(now); break;
    case MODE_WAVE:  updateWave(now);  break;
    case MODE_SWEEP: updateSweep(dt);  break;
  }
}

// ===========================================================================
//  TRIPOD GAIT WALK
//  Groups: A = Legs 1,3,5 (indices 0,2,4)   B = Legs 2,4,6 (indices 1,3,5)
//  Group A swings while B supports, then they switch.
// ===========================================================================
void updateWalk(unsigned long now) {
  float progress = (float)(now % walkPeriod) / (float)walkPeriod;   // 0.0 → 1.0

  for (int i = 0; i < NUM_LEGS; i++) {
    // Even indices (0,2,4) = Group A,  Odd indices (1,3,5) = Group B
    float phase = (i % 2 == 0) ? progress : fmodf(progress + 0.5f, 1.0f);

    // Left-side legs mirror the coxa swing direction
    float cDir = legs[i].leftSide ? -1.0f : 1.0f;

    float lA, mA, dA;

    if (phase < 0.5f) {
      // ---- Swing phase (leg in air, moving forward) ----
      float t    = phase / 0.5f;                        // 0 → 1
      float lift = sinf(t * PI);                        // arc 0→1→0
      lA = 90.0f + cDir * (-walkSwing + 2.0f * walkSwing * t);
      mA = 60.0f + walkLift * lift;                     // lift up
      dA = 120.0f - 20.0f * lift;                       // tuck foot
    } else {
      // ---- Stance phase (leg on ground, pushing back) ----
      float t = (phase - 0.5f) / 0.5f;                 // 0 → 1
      lA = 90.0f + cDir * (walkSwing - 2.0f * walkSwing * t);
      mA = 60.0f;                                      // hold down
      dA = 120.0f;                                      // hold planted
    }

    setLeg(i, lA, mA, dA);
  }
}

// ===========================================================================
//  DANCE ROUTINE  (16-second 4-phase choreography on all 6 legs)
// ===========================================================================
void updateDance(unsigned long now) {
  uint32_t cycle = now % 16000;                         // 16-second loop
  float tSec = (float)cycle / 1000.0f;

  for (int i = 0; i < NUM_LEGS; i++) {
    float cDir   = legs[i].leftSide ? -1.0f : 1.0f;
    float phOff  = (float)i * 0.3f;                     // stagger between legs
    float t      = tSec + phOff;

    float lA, mA, dA;

    // --- Phase 1 (0–4s): Hip Sway & Knee Bounce ---
    if (cycle < 4000) {
      lA = 90.0f + cDir * sinf(t * PI) * 35.0f;
      mA = 65.0f + fabsf(sinf(t * 2.0f * PI)) * 30.0f;
      dA = 110.0f + sinf(t * 2.0f * PI) * 20.0f;
    }
    // --- Phase 2 (4–8s): Rapid Toe Tap ---
    else if (cycle < 8000) {
      float st = t - 4.0f;
      lA = 70.0f + cDir * (fmodf(fabsf(st), 4.0f) / 4.0f) * 40.0f;
      mA = 80.0f + sinf(st * PI) * 10.0f;
      dA = 95.0f + (sinf(st * 6.0f * PI) > 0.0f ? 35.0f : 0.0f);
    }
    // --- Phase 3 (8–12s): Can-Can High Kick ---
    else if (cycle < 12000) {
      float st     = t - 8.0f;
      float kickPh = fmodf(fabsf(st), 2.0f);

      if (kickPh < 1.0f) {
        float kH = sinf(kickPh * PI);                   // kick arc
        lA = 90.0f + cDir * sinf(kickPh * 2.0f * PI) * 25.0f;
        mA = 60.0f + kH * 55.0f;                        // thigh lifts
        dA = 120.0f - kH * 60.0f;                       // shin extends
      } else {
        float sp = kickPh - 1.0f;
        lA = 90.0f + cDir * sinf(sp * 4.0f * PI) * 20.0f;  // shimmy
        mA = 50.0f;
        dA = 135.0f;
      }
    }
    // --- Phase 4 (12–16s): Snake Body Wave ---
    else {
      float st = t - 12.0f;
      float w  = st * 4.5f;
      lA = 90.0f + cDir * sinf(w) * 32.0f;
      mA = 75.0f + sinf(w - 1.05f) * 28.0f;
      dA = 105.0f + sinf(w - 2.10f) * 32.0f;
    }

    setLeg(i, lA, mA, dA);
  }
}

// ===========================================================================
//  WAVE  (sequential lift propagating across all 6 legs)
// ===========================================================================
void updateWave(unsigned long now) {
  float t = (float)(now % 3000) / 3000.0f;             // 3-second cycle

  for (int i = 0; i < NUM_LEGS; i++) {
    float phase = fmodf(t + (float)i / (float)NUM_LEGS, 1.0f);
    float lift  = sinf(phase * 2.0f * PI);
    lift = (lift > 0.0f) ? lift : 0.0f;                // only lift, no push below

    float cDir = legs[i].leftSide ? -1.0f : 1.0f;
    float lA = 90.0f + cDir * sinf(phase * 2.0f * PI) * 20.0f;
    float mA = 60.0f + lift * 35.0f;
    float dA = 120.0f - lift * 25.0f;

    setLeg(i, lA, mA, dA);
  }
}

// ===========================================================================
//  SWEEP  (all legs synchronised 45° ↔ 135°)
// ===========================================================================
void updateSweep(float dt) {
  sweepPos += sweepDir * sweepRate * dt;
  if (sweepPos >= 135.0f) { sweepPos = 135.0f; sweepDir = -1.0f; }
  if (sweepPos <=  45.0f) { sweepPos =  45.0f; sweepDir =  1.0f; }

  for (int i = 0; i < NUM_LEGS; i++) {
    setLeg(i, sweepPos, sweepPos, sweepPos);
  }
}

// ===========================================================================
//  I²C BUS SCANNER
// ===========================================================================
void scanI2C() {
  Serial.println(F("[SCAN] Scanning I2C bus..."));
  byte count = 0;
  for (byte addr = 8; addr < 120; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  -> 0x%02X", addr);
      if (addr == PCA_MAIN_ADDR) { Serial.print(" [PCA9685 Right (0x40)]"); boardMainOK = true; }
      if (addr == PCA_AUX_ADDR)  { Serial.print(" [PCA9685 Left (0x43)]");  boardAuxOK  = true; }
      if (addr == 0x70)          { Serial.print(" [PCA9685 All-Call]"); }
      Serial.println();
      count++;
    }
  }
  if (!boardMainOK) Serial.println(F("[WARN] PCA9685 Right (0x40) NOT detected!"));
  if (!boardAuxOK)  Serial.println(F("[WARN] PCA9685 Left  (0x43) NOT detected!"));
  if (count == 0)   Serial.println(F("[ERROR] No I2C devices found! Check 3V3/GND/SDA/SCL wiring."));
  Serial.printf("[SCAN] %d device(s) found.\n", count);
}

// ===========================================================================
//  COMMAND PROCESSOR  (USB Serial + Bluetooth)
// ===========================================================================
void processCmd(String cmd) {
  cmd.toUpperCase();
  Serial.printf(">> %s\n", cmd.c_str());
  SerialBT.printf(">> %s\n", cmd.c_str());

  // ======= Mode / Action Commands =======

  if (cmd == "HELP" || cmd == "?") {
    printHelp();
    return;
  }
  if (cmd == "STATUS" || cmd == "P") {
    printStatus();
    return;
  }
  if (cmd == "CENTER" || cmd == "C") {
    centerAll();
    Serial.println(F("[OK] All 18 servos centered to 90 deg"));
    SerialBT.println(F("OK:CENTER"));
    return;
  }
  if (cmd == "STAND") {
    standAll();
    Serial.println(F("[OK] Stand pose: L=90 M=60 D=120"));
    SerialBT.println(F("OK:STAND"));
    return;
  }
  if (cmd == "WALK" || cmd == "GAIT" || cmd == "STEP") {
    mode = MODE_WALK;
    Serial.println(F("[OK] Tripod gait walk active"));
    SerialBT.println(F("OK:WALK"));
    return;
  }
  if (cmd == "DANCE" || cmd == "PARTY") {
    mode = MODE_DANCE;
    Serial.println(F("[OK] Dance routine active (16-sec cycle)"));
    SerialBT.println(F("OK:DANCE"));
    return;
  }
  if (cmd == "WAVE") {
    mode = MODE_WAVE;
    Serial.println(F("[OK] Wave motion active"));
    SerialBT.println(F("OK:WAVE"));
    return;
  }
  if (cmd == "SWEEP" || cmd == "S") {
    mode = MODE_SWEEP;
    sweepPos = 90.0f;
    sweepDir = 1.0f;
    Serial.println(F("[OK] Sweep active (45-135 deg)"));
    SerialBT.println(F("OK:SWEEP"));
    return;
  }
  if (cmd == "STOP" || cmd == "X") {
    mode = MODE_HOLD;
    Serial.println(F("[OK] Stopped — holding position"));
    SerialBT.println(F("OK:STOP"));
    return;
  }
  if (cmd == "RELAX") {
    relaxAll();
    Serial.println(F("[OK] All servos relaxed (PWM off, limp)"));
    SerialBT.println(F("OK:RELAX"));
    return;
  }
  if (cmd == "SCAN") {
    boardMainOK = false;
    boardAuxOK  = false;
    scanI2C();
    // Re-init boards if newly detected
    if (boardMainOK) {
      pcaMain.begin();
      pcaMain.setOscillatorFrequency(OSC_FREQ);
      pcaMain.setPWMFreq(SERVO_FREQ);
    }
    if (boardAuxOK) {
      pcaAux.begin();
      pcaAux.setOscillatorFrequency(OSC_FREQ);
      pcaAux.setPWMFreq(SERVO_FREQ);
    }
    for (int i = 0; i < NUM_LEGS; i++) {
      bool ok = (legs[i].L.board == &pcaMain) ? boardMainOK : boardAuxOK;
      legs[i].L.active = ok;
      legs[i].M.active = ok;
      legs[i].D.active = ok;
    }
    return;
  }
  if (cmd == "SWAP") {
    for (int i = 0; i < NUM_LEGS; i++) {
      legs[i].L.board = (legs[i].L.board == &pcaMain) ? &pcaAux : &pcaMain;
      legs[i].M.board = (legs[i].M.board == &pcaMain) ? &pcaAux : &pcaMain;
      legs[i].D.board = (legs[i].D.board == &pcaMain) ? &pcaAux : &pcaMain;
      bool ok = (legs[i].L.board == &pcaMain) ? boardMainOK : boardAuxOK;
      legs[i].L.active = ok;
      legs[i].M.active = ok;
      legs[i].D.active = ok;
    }
    for (int i = 0; i < NUM_LEGS; i++) {
      writeJoint(legs[i].L, legs[i].L.target);
      writeJoint(legs[i].M, legs[i].M.target);
      writeJoint(legs[i].D, legs[i].D.target);
    }
    uint8_t lAddr = (legs[3].L.board == &pcaMain) ? PCA_MAIN_ADDR : PCA_AUX_ADDR;
    uint8_t rAddr = (legs[0].L.board == &pcaMain) ? PCA_MAIN_ADDR : PCA_AUX_ADDR;
    Serial.printf("[OK] Swapped PCA boards! Left (Legs 4-6) on 0x%02X, Right (Legs 1-3) on 0x%02X\n", lAddr, rAddr);
    SerialBT.printf("OK:SWAP L=0x%02X R=0x%02X\n", lAddr, rAddr);
    return;
  }

  // ======= Individual Joint: L1–L6, M1–M6, D1–D6 =======
  // Format: "L3 45"  or  "D6 120"
  if (cmd.length() >= 4
      && (cmd[0] == 'L' || cmd[0] == 'M' || cmd[0] == 'D')
      && cmd[1] >= '1' && cmd[1] <= '6'
      && cmd[2] == ' ') {
    char jt  = cmd[0];
    int  idx = cmd[1] - '1';               // 0-based leg index
    float a  = cmd.substring(3).toFloat();
    mode = MODE_HOLD;

    Joint* j = (jt == 'L') ? &legs[idx].L :
               (jt == 'M') ? &legs[idx].M : &legs[idx].D;
    writeJoint(*j, a);
    Serial.printf("[OK] %c%d -> %.1f deg (ch%d on 0x%02X)\n",
                  jt, idx + 1, a, j->ch,
                  (j->board == &pcaMain) ? PCA_MAIN_ADDR : PCA_AUX_ADDR);
    SerialBT.printf("OK:%c%d=%.1f\n", jt, idx + 1, a);
    return;
  }

  // Legacy single-joint: "L 45" → defaults to Leg 1
  if (cmd.length() >= 3
      && (cmd[0] == 'L' || cmd[0] == 'M' || cmd[0] == 'D')
      && cmd[1] == ' ') {
    char jt = cmd[0];
    float a = cmd.substring(2).toFloat();
    mode = MODE_HOLD;

    Joint* j = (jt == 'L') ? &legs[0].L :
               (jt == 'M') ? &legs[0].M : &legs[0].D;
    writeJoint(*j, a);
    Serial.printf("[OK] %c1 -> %.1f deg (default leg 1)\n", jt, a);
    SerialBT.printf("OK:%c1=%.1f\n", jt, a);
    return;
  }

  // ======= Full Leg: "LEG 3 90 60 120" =======
  if (cmd.startsWith("LEG ")) {
    int n;
    float lv, mv, dv;
    if (sscanf(cmd.c_str(), "LEG %d %f %f %f", &n, &lv, &mv, &dv) == 4) {
      if (n >= 1 && n <= 6) {
        mode = MODE_HOLD;
        setLeg(n - 1, lv, mv, dv);
        Serial.printf("[OK] Leg %d -> L=%.1f M=%.1f D=%.1f\n", n, lv, mv, dv);
        SerialBT.printf("OK:LEG%d\n", n);
      } else {
        Serial.println(F("[ERR] Leg number must be 1-6"));
      }
    } else {
      Serial.println(F("[ERR] Usage: LEG <1-6> <L_deg> <M_deg> <D_deg>"));
    }
    return;
  }

  // ======= SET all legs: "SET 90 60 120" =======
  if (cmd.startsWith("SET ")) {
    float lv, mv, dv;
    if (sscanf(cmd.c_str(), "SET %f %f %f", &lv, &mv, &dv) == 3) {
      mode = MODE_HOLD;
      for (int i = 0; i < NUM_LEGS; i++) setLeg(i, lv, mv, dv);
      Serial.printf("[OK] All legs -> L=%.1f M=%.1f D=%.1f\n", lv, mv, dv);
      SerialBT.println(F("OK:SET"));
    } else {
      Serial.println(F("[ERR] Usage: SET <L_deg> <M_deg> <D_deg>"));
    }
    return;
  }

  // ======= ALL servos to one angle: "ALL 90" =======
  if (cmd.startsWith("ALL ")) {
    float a = cmd.substring(4).toFloat();
    mode = MODE_HOLD;
    for (int i = 0; i < NUM_LEGS; i++) setLeg(i, a, a, a);
    uint16_t us = (uint16_t)map((long)(a * 10.0f), 0L, 1800L, PULSE_MIN, PULSE_MAX);
    uint16_t tick = (uint16_t)((float)us * 4096.0f / 20000.0f + 0.5f);
    tick = constrain(tick, (uint16_t)100, (uint16_t)550);
    if (boardMainOK) {
      for (uint8_t ch = 0; ch < 16; ch++) pcaMain.setPWM(ch, 0, tick);
    }
    if (boardAuxOK) {
      for (uint8_t ch = 0; ch < 16; ch++) pcaAux.setPWM(ch, 0, tick);
    }
    Serial.printf("[OK] All servos -> %.1f deg (tick=%d on ch 0-15)\n", a, tick);
    SerialBT.printf("OK:ALL=%.1f\n", a);
    return;
  }

  // ======= TRIM calibration: "TRIM L1 +5" or just "TRIM" to view =======
  if (cmd.startsWith("TRIM")) {
    // "TRIM L1 +5" → cmd[5]='L', cmd[6]='1', cmd[7]=' ', cmd[8..]="+5"
    if (cmd.length() >= 9
        && (cmd[5] == 'L' || cmd[5] == 'M' || cmd[5] == 'D')
        && cmd[6] >= '1' && cmd[6] <= '6'
        && cmd[7] == ' ') {
      char jt  = cmd[5];
      int  idx = cmd[6] - '1';
      float offset = cmd.substring(8).toFloat();

      Joint* j = (jt == 'L') ? &legs[idx].L :
                 (jt == 'M') ? &legs[idx].M : &legs[idx].D;
      j->trim = offset;
      writeJoint(*j, j->target);            // Re-apply with new trim
      Serial.printf("[OK] TRIM %c%d = %+.1f deg\n", jt, idx + 1, offset);
      SerialBT.printf("OK:TRIM_%c%d=%+.1f\n", jt, idx + 1, offset);
    } else {
      // Show all current trims
      Serial.println(F("\n--- TRIM OFFSETS ---"));
      for (int i = 0; i < NUM_LEGS; i++) {
        Serial.printf("  Leg %d %-12s  L=%+5.1f  M=%+5.1f  D=%+5.1f\n",
                      legs[i].num, legs[i].name,
                      legs[i].L.trim, legs[i].M.trim, legs[i].D.trim);
      }
      Serial.println(F("Usage: TRIM <L|M|D><1-6> <offset>  e.g. TRIM L1 +5"));
    }
    return;
  }

  // ======= INVERT direction: "INV L1" or just "INV" to view =======
  if (cmd.startsWith("INV")) {
    if (cmd.length() >= 6
        && (cmd[4] == 'L' || cmd[4] == 'M' || cmd[4] == 'D')
        && cmd[5] >= '1' && cmd[5] <= '6') {
      char jt  = cmd[4];
      int  idx = cmd[5] - '1';

      Joint* j = (jt == 'L') ? &legs[idx].L :
                 (jt == 'M') ? &legs[idx].M : &legs[idx].D;
      j->inv = !j->inv;
      writeJoint(*j, j->target);
      Serial.printf("[OK] %c%d direction: %s\n",
                    jt, idx + 1, j->inv ? "INVERTED (180-deg)" : "NORMAL");
      SerialBT.printf("OK:INV_%c%d=%d\n", jt, idx + 1, j->inv ? 1 : 0);
    } else {
      Serial.println(F("\n--- JOINT DIRECTION INVERSIONS ---"));
      for (int i = 0; i < NUM_LEGS; i++) {
        Serial.printf("  Leg %d %-12s  L=%-4s  M=%-4s  D=%-4s\n",
                      legs[i].num, legs[i].name,
                      legs[i].L.inv ? "INV" : "NORM",
                      legs[i].M.inv ? "INV" : "NORM",
                      legs[i].D.inv ? "INV" : "NORM");
      }
      Serial.println(F("Usage: INV <L|M|D><1-6>  e.g. INV M1"));
    }
    return;
  }

  // ======= Walk tuning: "WALKSPEED 1500" / "WALKSWING 30" / "WALKLIFT 40" =======
  if (cmd.startsWith("WALKSPEED ")) {
    walkPeriod = (uint32_t)cmd.substring(10).toInt();
    if (walkPeriod < 500)  walkPeriod = 500;
    if (walkPeriod > 8000) walkPeriod = 8000;
    Serial.printf("[OK] Walk period = %lu ms\n", walkPeriod);
    SerialBT.printf("OK:WALKSPEED=%lu\n", walkPeriod);
    return;
  }
  if (cmd.startsWith("WALKSWING ")) {
    walkSwing = cmd.substring(10).toFloat();
    walkSwing = constrain(walkSwing, 5.0f, 45.0f);
    Serial.printf("[OK] Walk swing = %.1f deg\n", walkSwing);
    return;
  }
  if (cmd.startsWith("WALKLIFT ")) {
    walkLift = cmd.substring(9).toFloat();
    walkLift = constrain(walkLift, 10.0f, 60.0f);
    Serial.printf("[OK] Walk lift = %.1f deg\n", walkLift);
    return;
  }

  // ======= Unknown =======
  Serial.println(F("[ERR] Unknown command. Type HELP or ?"));
  SerialBT.println(F("ERR:UNKNOWN"));
}

// ===========================================================================
//  HELP MENU
// ===========================================================================
void printHelp() {
  Serial.println(F("\n----------------------------------------------------------"));
  Serial.println(F("  HEXAPOD COMMAND REFERENCE  (USB Serial + Bluetooth)"));
  Serial.println(F("----------------------------------------------------------"));
  Serial.println(F(" Motion Modes:"));
  Serial.println(F("   WALK / GAIT / STEP  -> Tripod gait walk"));
  Serial.println(F("   DANCE / PARTY       -> 16-sec choreographed dance"));
  Serial.println(F("   WAVE                -> Sequential leg wave"));
  Serial.println(F("   SWEEP / S           -> Synchronised sweep 45-135 deg"));
  Serial.println(F("   STOP / X            -> Stop motion, hold position"));
  Serial.println(F("   RELAX               -> Disable PWM (servos go limp)"));
  Serial.println(F(""));
  Serial.println(F(" Poses:"));
  Serial.println(F("   CENTER / C          -> All servos to 90 deg"));
  Serial.println(F("   STAND               -> Standing stance (90/60/120)"));
  Serial.println(F(""));
  Serial.println(F(" Individual Joint Control:"));
  Serial.println(F("   L1 <deg> ... L6 <deg>   -> Set Coxa angle"));
  Serial.println(F("   M1 <deg> ... M6 <deg>   -> Set Femur angle"));
  Serial.println(F("   D1 <deg> ... D6 <deg>   -> Set Tibia angle"));
  Serial.println(F("   L <deg>  / M <deg> / D <deg> -> Default to Leg 1"));
  Serial.println(F(""));
  Serial.println(F(" Multi-Joint:"));
  Serial.println(F("   LEG <n> <L> <M> <D> -> Set all 3 joints of leg n"));
  Serial.println(F("   SET <L> <M> <D>     -> Set all 6 legs to same pose"));
  Serial.println(F("   ALL <deg>           -> Set all 18 servos to one angle"));
  Serial.println(F(""));
  Serial.println(F(" Calibration & Inversion:"));
  Serial.println(F("   TRIM <J><n> <offset> -> Set trim (e.g. TRIM L1 +5)"));
  Serial.println(F("   TRIM                 -> Show all current trims"));
  Serial.println(F("   INV <J><n>           -> Toggle direction inversion (e.g. INV M1)"));
  Serial.println(F("   INV                  -> Show all joint inversion states"));
  Serial.println(F(""));
  Serial.println(F(" Walk Tuning:"));
  Serial.println(F("   WALKSPEED <ms>       -> Gait period (500-8000, def 2000)"));
  Serial.println(F("   WALKSWING <deg>      -> Coxa swing range (5-45, def 25)"));
  Serial.println(F("   WALKLIFT <deg>       -> Femur lift height (10-60, def 30)"));
  Serial.println(F(""));
  Serial.println(F(" Diagnostics:"));
  Serial.println(F("   STATUS / P          -> Full system status"));
  Serial.println(F("   SCAN                -> Re-scan I2C bus"));
  Serial.println(F("   SWAP                -> Swap Right & Left PCA board IDs live"));
  Serial.println(F("   HELP / ?            -> This menu"));
  Serial.println(F("----------------------------------------------------------\n"));
}

// ===========================================================================
//  STATUS DISPLAY
// ===========================================================================
void printStatus() {
  Serial.println(F("\n=================== HEXAPOD STATUS ===================="));
  Serial.printf("  Board 0x40 (Right): %s    Board 0x43 (Left): %s\n",
                boardMainOK ? "OK" : "MISSING",
                boardAuxOK  ? "OK" : "MISSING");
  Serial.println(F("-------------------------------------------------------"));

  for (int i = 0; i < NUM_LEGS; i++) {
    uint8_t addr = (legs[i].L.board == &pcaMain) ? PCA_MAIN_ADDR : PCA_AUX_ADDR;
    Serial.printf(" Leg %d  %-12s  [0x%02X %s]\n",
                  legs[i].num, legs[i].name, addr,
                  legs[i].L.active ? "OK" : "--");
    Serial.printf("   L(Coxa) :ch%d = %5.1f deg  trim=%+5.1f  dir=%s\n",
                  legs[i].L.ch, legs[i].L.angle, legs[i].L.trim, legs[i].L.inv ? "INV" : "NORM");
    Serial.printf("   M(Femur):ch%d = %5.1f deg  trim=%+5.1f  dir=%s\n",
                  legs[i].M.ch, legs[i].M.angle, legs[i].M.trim, legs[i].M.inv ? "INV" : "NORM");
    Serial.printf("   D(Tibia):ch%d = %5.1f deg  trim=%+5.1f  dir=%s\n",
                  legs[i].D.ch, legs[i].D.angle, legs[i].D.trim, legs[i].D.inv ? "INV" : "NORM");
  }

  Serial.println(F("-------------------------------------------------------"));
  const char* modeStr =
    (mode == MODE_HOLD)  ? "HOLD (static)" :
    (mode == MODE_WALK)  ? "WALK (tripod gait)" :
    (mode == MODE_DANCE) ? "DANCE (16-sec cycle)" :
    (mode == MODE_WAVE)  ? "WAVE (sequential)" : "SWEEP (45-135)";
  Serial.printf(" Mode: %s\n", modeStr);
  Serial.printf(" Walk: period=%lu ms  swing=%.0f deg  lift=%.0f deg\n",
                walkPeriod, walkSwing, walkLift);
  Serial.println(F("=======================================================\n"));
}
