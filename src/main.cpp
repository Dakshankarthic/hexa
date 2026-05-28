// =============================================================================
// ESP32 Hexapod - Full 18-Servo Firmware with Wi-Fi Web Control
// PlatformIO | Framework: Arduino | Board: ESP32 DevKit
//
// Hardware: ESP32 30-pin + 2x PCA9685 + 18x MG996R (180° servos)
// Board 1 (0x40): Right legs (0, 1, 2) | Board 2 (0x41): Left legs (3, 4, 5)
// =============================================================================

#include <Adafruit_PWMServoDriver.h>
#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>
#include <math.h>

// ---------------------------------------------------------------------------
// Wi-Fi Access Point Credentials
// ---------------------------------------------------------------------------
const char *ssid = "DK 4412";
const char *password = "197M97a>"; // Must be at least 8 characters

WebServer server(80);

// ---------------------------------------------------------------------------
// PCA9685 Board instances
// ---------------------------------------------------------------------------
Adafruit_PWMServoDriver boardRight =
    Adafruit_PWMServoDriver(0x40);                                 // Right legs
Adafruit_PWMServoDriver boardLeft = Adafruit_PWMServoDriver(0x41); // Left legs

#define SERVO_FREQ 50          // Hz - standard for analog servos
#define OSC_FREQUENCY 27000000 // PCA9685 oscillator

// ---------------------------------------------------------------------------
// Servo pulse limits (microseconds) for 180° servos (MG996R)
// ---------------------------------------------------------------------------
#define PULSE_MIN 500  // ~0 degrees
#define PULSE_MID 1500 // ~90 degrees (mechanical neutral)
#define PULSE_MAX 2500 // ~180 degrees

// ---------------------------------------------------------------------------
// Leg geometry (millimeters) - adjust to your 3D-printed dimensions
// ---------------------------------------------------------------------------
#define COXA_LEN 52.0f   // Shoulder pivot → femur pivot
#define FEMUR_LEN 66.0f  // Femur pivot → tibia pivot
#define TIBIA_LEN 130.0f // Tibia pivot → foot tip

// ---------------------------------------------------------------------------
// Leg standing position (body-relative, mm)
// ---------------------------------------------------------------------------
#define STAND_X 100.0f  // Horizontal reach from body center
#define STAND_Y 0.0f    // Forward/back offset at neutral
#define STAND_Z -110.0f // Height (negative = downward)

// ---------------------------------------------------------------------------
// Gait parameters
// ---------------------------------------------------------------------------
#define STEP_HEIGHT 30.0f // mm the foot lifts during swing
#define STEP_LENGTH 40.0f // mm of forward travel per step
#define TURN_LENGTH 30.0f // mm of lateral travel for turning
#define GAIT_SPEED 20     // ms delay for smooth movement

// ---------------------------------------------------------------------------
// Leg indexing & configuration
// ---------------------------------------------------------------------------
#define NUM_LEGS 6
#define DOF 3 // Coxa, Femur, Tibia per leg

struct LegConfig {
  bool isLeft;      // Mirror coxa direction for left-side legs
  float mountAngle; // Body-frame mount angle (degrees) for foot placement
};

const LegConfig LEG_CFG[NUM_LEGS] = {
    {false, 45.0f},  // 0: Front Right
    {false, 0.0f},   // 1: Mid   Right
    {false, -45.0f}, // 2: Rear  Right
    {true, 135.0f},  // 3: Front Left
    {true, 180.0f},  // 4: Mid   Left
    {true, 225.0f},  // 5: Rear  Left
};

// Tripod gait groups
const int TRIPOD_A[3] = {0, 4, 2}; // FR, ML, RR
const int TRIPOD_B[3] = {1, 3, 5}; // MR, FL, RL

// Robot State Machine
enum RobotState { IDLE, WALK_FWD, WALK_BWD, TURN_LEFT, TURN_RIGHT };
RobotState currentState = IDLE;

// ===========================================================================
// FORWARD DECLARATIONS
// ===========================================================================
void setPulse(uint8_t leg, uint8_t joint, uint16_t pulseMicros);
void setAngle(uint8_t leg, uint8_t joint, float degrees);
bool solveIK(uint8_t leg, float x, float y, float z, float &coxaDeg,
             float &femurDeg, float &tibiaDeg);
void moveLegTo(uint8_t leg, float x, float y, float z);
void standUp();
void sitDown();
void tripodStep(float dx, float dy);
void setupWiFi();
void handleRoot();
void handleCommand();

// ===========================================================================
// SETUP
// ===========================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=================================================");
  Serial.println(" ESP32 Hexapod - Full 18-Servo Wi-Fi Firmware");
  Serial.println("=================================================");

  // Initialize I2C
  Wire.begin(21, 22);
  Wire.setClock(400000);

  // Initialize PCA9685 boards
  boardRight.begin();
  boardRight.setOscillatorFrequency(OSC_FREQUENCY);
  boardRight.setPWMFreq(SERVO_FREQ);
  boardLeft.begin();
  boardLeft.setOscillatorFrequency(OSC_FREQUENCY);
  boardLeft.setPWMFreq(SERVO_FREQ);
  Serial.println("[OK] I2C & PCA9685 initialized.");
  delay(200);

  // Setup Wi-Fi Access Point & Web Server
  setupWiFi();

  // Initial stance
  standUp();
}

// ===========================================================================
// MAIN LOOP
// ===========================================================================
void loop() {
  server.handleClient(); // Listen for Wi-Fi commands

  // Execute continuous movement based on current state
  switch (currentState) {
  case WALK_FWD:
    tripodStep(STEP_LENGTH, 0.0f);
    break;
  case WALK_BWD:
    tripodStep(-STEP_LENGTH, 0.0f);
    break;
  case TURN_LEFT:
    tripodStep(0.0f, -TURN_LENGTH);
    break;
  case TURN_RIGHT:
    tripodStep(0.0f, TURN_LENGTH);
    break;
  case IDLE:
  default:
    break; // Do nothing, hold position
  }
}

// ===========================================================================
// WEB SERVER INTERFACE
// ===========================================================================
void setupWiFi() {
  Serial.println("[>>] Starting Wi-Fi Access Point...");
  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("[OK] Wi-Fi started! Connect to '");
  Serial.print(ssid);
  Serial.println("' with phone.");
  Serial.print("     Then open browser to: http://");
  Serial.println(IP);

  server.on("/", handleRoot);
  server.on("/cmd", handleCommand);
  server.begin();
  Serial.println("[OK] Web Server running.");
}

void handleRoot() {
  // Mobile-friendly HTML Gamepad UI
  String html = "<!DOCTYPE html><html><head><meta name='viewport' "
                "content='width=device-width, initial-scale=1, "
                "maximum-scale=1, user-scalable=no'>";
  html += "<title>Hexapod Control</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; background-color: #1a1a1a; "
          "color: #fff; text-align: center; margin: 0; padding: 20px; "
          "user-select: none; }";
  html += "h2 { color: #00ffcc; }";
  html += ".grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: "
          "15px; max-width: 400px; margin: 40px auto; }";
  html +=
      ".btn { background-color: #333; border: 2px solid #555; border-radius: "
      "12px; color: white; padding: 25px 0; font-size: 24px; font-weight: "
      "bold; cursor: pointer; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }";
  html += ".btn:active { background-color: #00ffcc; color: #000; transform: "
          "translateY(4px); box-shadow: none; }";
  html +=
      ".btn-stop { background-color: #ff3333; grid-column: 2; grid-row: 2; }";
  html += ".btn-empty { visibility: hidden; }";
  html += ".actions { display: flex; justify-content: center; gap: 20px; "
          "margin-top: 30px; }";
  html += ".btn-action { background-color: #007bff; border-radius: 8px; "
          "padding: 15px 30px; font-size: 18px; border: none; color: white; }";
  html += "</style>";
  html += "<script>";
  html += "function sendCmd(cmd) { fetch('/cmd?c=' + cmd); }";
  html += "</script>";
  html += "</head><body>";

  html += "<h2>🕷️ Hexapod Spider Controller</h2>";

  // Directional Pad
  html += "<div class='grid'>";
  html += "<div class='btn-empty'></div>";
  html += "<button class='btn' onmousedown=\"sendCmd('F')\" "
          "onmouseup=\"sendCmd('X')\" ontouchstart=\"sendCmd('F')\" "
          "ontouchend=\"sendCmd('X')\">▲</button>";
  html += "<div class='btn-empty'></div>";

  html += "<button class='btn' onmousedown=\"sendCmd('L')\" "
          "onmouseup=\"sendCmd('X')\" ontouchstart=\"sendCmd('L')\" "
          "ontouchend=\"sendCmd('X')\">◄</button>";
  html += "<button class='btn btn-stop' onclick=\"sendCmd('X')\">STOP</button>";
  html += "<button class='btn' onmousedown=\"sendCmd('R')\" "
          "onmouseup=\"sendCmd('X')\" ontouchstart=\"sendCmd('R')\" "
          "ontouchend=\"sendCmd('X')\">►</button>";

  html += "<div class='btn-empty'></div>";
  html += "<button class='btn' onmousedown=\"sendCmd('B')\" "
          "onmouseup=\"sendCmd('X')\" ontouchstart=\"sendCmd('B')\" "
          "ontouchend=\"sendCmd('X')\">▼</button>";
  html += "<div class='btn-empty'></div>";
  html += "</div>";

  // Action Buttons
  html += "<div class='actions'>";
  html +=
      "<button class='btn-action' onclick=\"sendCmd('U')\">Stand Up</button>";
  html += "<button class='btn-action' style='background-color:#555;' "
          "onclick=\"sendCmd('D')\">Sit Down</button>";
  html += "</div>";

  html += "<p style='margin-top: 40px; font-size: 12px; color: #888;'>Hold "
          "buttons to move. Release to stop.</p>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleCommand() {
  if (server.hasArg("c")) {
    String cmd = server.arg("c");
    Serial.print("[Web CMD] Received: ");
    Serial.println(cmd);

    if (cmd == "F")
      currentState = WALK_FWD;
    else if (cmd == "B")
      currentState = WALK_BWD;
    else if (cmd == "L")
      currentState = TURN_LEFT;
    else if (cmd == "R")
      currentState = TURN_RIGHT;
    else if (cmd == "X") {
      currentState = IDLE;
      standUp();
    } else if (cmd == "U") {
      currentState = IDLE;
      standUp();
    } else if (cmd == "D") {
      currentState = IDLE;
      sitDown();
    }
  }
  server.send(200, "text/plain", "OK");
}

// ===========================================================================
// MOVEMENT MACROS
// ===========================================================================

void standUp() {
  for (uint8_t leg = 0; leg < NUM_LEGS; leg++) {
    moveLegTo(leg, STAND_X, STAND_Y, STAND_Z);
  }
  delay(100);
}

void sitDown() {
  for (uint8_t leg = 0; leg < NUM_LEGS; leg++) {
    moveLegTo(leg, STAND_X * 0.7f, STAND_Y, STAND_Z * 0.4f);
  }
  delay(100);
}

// ===========================================================================
// TRIPOD GAIT (Single Cycle)
// ===========================================================================
void tripodStep(float dx, float dy) {
  float footX[NUM_LEGS], footY[NUM_LEGS], footZ[NUM_LEGS];

  // Base positions
  for (uint8_t leg = 0; leg < NUM_LEGS; leg++) {
    float mountRad = LEG_CFG[leg].mountAngle * DEG_TO_RAD;
    footX[leg] = STAND_X;
    footY[leg] = STAND_Y;
    footZ[leg] = STAND_Z;
  }

  // --- PHASE 1: Lift Group A, Push Group B ---
  for (int i = 0; i < 3; i++) {
    uint8_t leg = TRIPOD_A[i];
    moveLegTo(leg, footX[leg] + dx * 0.5f, footY[leg] + dy * 0.5f,
              STAND_Z + STEP_HEIGHT);
  }
  server.handleClient();
  delay(GAIT_SPEED * 3);

  for (int i = 0; i < 3; i++) {
    uint8_t legA = TRIPOD_A[i], legB = TRIPOD_B[i];
    moveLegTo(legA, footX[legA] + dx, footY[legA] + dy,
              STAND_Z); // A Lands ahead
    moveLegTo(legB, footX[legB] - dx, footY[legB] - dy,
              STAND_Z); // B Pushes back
  }
  server.handleClient();
  delay(GAIT_SPEED * 3);

  // --- PHASE 2: Lift Group B, Push Group A ---
  for (int i = 0; i < 3; i++) {
    uint8_t leg = TRIPOD_B[i];
    moveLegTo(leg, footX[leg] + dx * 0.5f, footY[leg] + dy * 0.5f,
              STAND_Z + STEP_HEIGHT);
  }
  server.handleClient();
  delay(GAIT_SPEED * 3);

  for (int i = 0; i < 3; i++) {
    uint8_t legA = TRIPOD_A[i], legB = TRIPOD_B[i];
    moveLegTo(legB, footX[legB] + dx, footY[legB] + dy,
              STAND_Z); // B Lands ahead
    moveLegTo(legA, footX[legA] - dx, footY[legA] - dy,
              STAND_Z); // A Pushes back
  }
  server.handleClient();
  delay(GAIT_SPEED * 3);
}

// ===========================================================================
// INVERSE KINEMATICS (IK)
// ===========================================================================
bool solveIK(uint8_t leg, float x, float y, float z, float &coxaDeg,
             float &femurDeg, float &tibiaDeg) {
  coxaDeg = atan2f(y, x) * RAD_TO_DEG;
  float L = sqrtf(x * x + y * y) - COXA_LEN;
  float D = sqrtf(L * L + z * z);

  float maxReach = FEMUR_LEN + TIBIA_LEN - 1.0f;
  if (D > maxReach)
    D = maxReach; // Clamp

  float cosT = (D * D - FEMUR_LEN * FEMUR_LEN - TIBIA_LEN * TIBIA_LEN) /
               (2.0f * FEMUR_LEN * TIBIA_LEN);
  tibiaDeg = acosf(constrain(cosT, -1.0f, 1.0f)) * RAD_TO_DEG;

  float alpha = atan2f(-z, L) * RAD_TO_DEG;
  float beta =
      asinf((TIBIA_LEN * sinf(tibiaDeg * DEG_TO_RAD)) / D) * RAD_TO_DEG;
  femurDeg = alpha + beta;

  // Map to servo neutral (90°)
  coxaDeg += 90.0f;
  femurDeg += 90.0f;
  tibiaDeg = 180.0f - tibiaDeg; // Invert for knee-forward

  if (LEG_CFG[leg].isLeft)
    coxaDeg = 180.0f - coxaDeg; // Mirror left legs

  coxaDeg = constrain(coxaDeg, 0.0f, 180.0f);
  femurDeg = constrain(femurDeg, 0.0f, 180.0f);
  tibiaDeg = constrain(tibiaDeg, 0.0f, 180.0f);
  return true;
}

void moveLegTo(uint8_t leg, float x, float y, float z) {
  float coxa, femur, tibia;
  if (!solveIK(leg, x, y, z, coxa, femur, tibia))
    return;

  setAngle(leg, 0, coxa);
  setAngle(leg, 1, femur);
  setAngle(leg, 2, tibia);
}

// ===========================================================================
// SERVO HARDWARE CONTROL
// ===========================================================================
void setAngle(uint8_t leg, uint8_t joint, float degrees) {
  uint16_t pulse = map((long)degrees, 0, 180, PULSE_MIN, PULSE_MAX);
  setPulse(leg, joint, pulse);
}

void setPulse(uint8_t leg, uint8_t joint, uint16_t pulseMicros) {
  uint8_t channel = (leg % 3) * 3 + joint;
  if (leg < 3)
    boardRight.writeMicroseconds(channel, pulseMicros);
  else
    boardLeft.writeMicroseconds(channel, pulseMicros);
}
