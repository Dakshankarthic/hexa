# 🕷️ ESP32 Hexapod Spider Robot

> **Project Owner:** Dakshan  
> **Start Date:** April 2026  
> **Last Updated:** 2026-05-01 00:44 IST  
> **Previous Updates:** 2026-05-01 00:30 IST | 2026-04-30 23:55 IST | 2026-04-30 20:00 IST
> **Phase 2 Status:** Finished on 2026-05-01 — Single-Leg Subassembly Testing (3× 360° servos)
> **Current Status:** Phase 3/4 — 18-Servo IK Firmware & Wi-Fi Control Implementation

---

## 📋 Table of Contents

- [Project Overview](#project-overview)
- [Design Evolution & Decisions](#design-evolution--decisions)
- [Hardware Bill of Materials](#hardware-bill-of-materials)
- [Architecture & Wiring](#architecture--wiring)
- [Power System](#power-system)
- [Software & Firmware](#software--firmware)
- [Current Progress](#current-progress)
- [Known Issues & Solutions](#known-issues--solutions)
- [Build Phases](#build-phases)
- [Information Log (Chronological)](#information-log-chronological)
- [Rules for AI Assistant](#-rules-for-ai-assistant)

---

## Project Overview

A **3D-printed 18-DOF hexapod spider robot** inspired by [Emre Kalem's RC Hexapod Spider Robot](https://www.youtube.com/) YouTube project. The original design has been **modernized and made cheaper** by replacing the Arduino Mega + Uno + NRF24L01 stack with a single ESP32 and PCA9685 I²C servo drivers.

### Key Specs (Target)

| Parameter | Value |
|-----------|-------|
| Legs | 6 (hexapod) |
| DOF per leg | 3 (Coxa, Femur, Tibia) |
| Total servos | 18× MG996R |
| MCU | ESP32 30-pin DevKit |
| Servo drivers | 2× PCA9685 (0x40 Right, 0x41 Left) |
| Communication | Wi-Fi / BLE (no NRF24L01) |
| Battery | 3S LiPo 11.1V |
| Frame | 3D printed |

---

## Design Evolution & Decisions

### 1. MCU Upgrade *(decided ~mid-April 2026)*

| Original (Emre's) | Your Design |
|--------------------|-------------|
| Arduino Mega (robot brain) | **ESP32 30-pin DevKit** |
| Arduino Uno (remote) | Phone / second ESP32 via Wi-Fi/BLE |
| NRF24L01 radio modules | Built-in ESP32 Wi-Fi/BLE |
| Direct PWM pins (18 pins) | **PCA9685 I²C servo drivers** |

**Rationale:** Cheaper, fewer boards, built-in wireless, more processing power for IK/gait.

### 2. Leg Count *(decided ~late April 2026)*

- Initially planned **6 legs (hexapod)** to match Emre's design.
- Briefly explored **4 legs (quadruped, 12 servos)** — confirmed feasible.
- **Final decision: 6 legs, 18 servos** — staying with hexapod.

### 3. Servo Driver Architecture *(decided ~late April 2026)*

- Single PCA9685 = 16 channels → not enough for 18 servos.
- **Solution: 2× PCA9685** boards on shared I²C bus.
  - Board 1 (0x40, default): **Right legs** (legs 0, 1, 2)
  - Board 2 (0x41, A0 bridged): **Left legs** (legs 3, 4, 5)
- Symmetric layout makes code cleaner and allows easy power splitting.

### 4. Power Routing *(decided ~late April 2026)*

- ESP32 expansion board accepts **6.5–16V DC input** → powered directly from 3S LiPo.
- No separate 5V buck needed for logic.
- Servos get power from **separate high-current buck converter** (ZX-052).

---

## Hardware Bill of Materials

### Control & Logic

| Component | Quantity | Specs | Notes |
|-----------|----------|-------|-------|
| ESP32 30-pin DevKit | 1 | Dual-core 240MHz, Wi-Fi/BLE | On expansion board |
| ESP32 Expansion Board | 1 | 6.5–16V DC input, 3.3V/5V regulator | Powers ESP32 directly from 3S LiPo |
| PCA9685 16-ch Servo Driver | 2 | I²C, 0x40 (default) & 0x41 (A0 bridged) | One per side (Right/Left) |

### Actuators

| Component | Quantity | Specs | Notes |
|-----------|----------|-------|-------|
| MG996R Servo (180°) | 18 (target) | Metal gear, ~10kg·cm torque | **Not yet purchased** — using 360° for now |
| 360° Continuous Rotation Servo | 3 (current) | Speed control, not position | For initial testing only |

### Power

| Component | Quantity | Specs | Notes |
|-----------|----------|-------|-------|
| Bonka 3S LiPo | 1 | 11.1V, 2200mAh, 35C (77A cont.) | XT60 + balance plug |
| iMAX B3 Pro AC Charger | 1 | 10W, 2S/3S balance charger, ~800mA | Gets warm — charge on non-flammable surface |
| ZX-052 Buck Converter | 1 | Adjustable DC-DC, ~15–20A class | Set to **6.0V** output for servos |

### Mechanical

| Component | Quantity | Notes |
|-----------|----------|-------|
| 3D-Printed Frame | 1 set | Inspired by Emre's hexapod design |
| 695 Bearings | Multiple | For leg joints |
| M3 Screws, Nuts, Standoffs | Various | Mounting electronics and joints |

---

## Architecture & Wiring

### I²C Bus (ESP32 → PCA9685 Boards)

```
ESP32 3V3  ──── VCC (Board 1) ──── VCC (Board 2)    [Logic power ONLY]
ESP32 GND  ──── GND (Board 1) ──── GND (Board 2)    [Common ground]
ESP32 D21  ──── SDA (Board 1) ──── SDA (Board 2)    [I²C Data]
ESP32 D22  ──── SCL (Board 1) ──── SCL (Board 2)    [I²C Clock]
```

> ⚠️ **NEVER connect ESP32 3V3 to PCA9685 V+!** V+ gets 6V from the ZX-052 buck.

### Power Wiring

```
Bonka 3S LiPo (11.1V)
    │
    ├──[Fuse 20-30A]──[Switch]──► ZX-052 Buck IN+ ──► OUT+ (6.0V) ──► PCA9685 V+ (both boards)
    │                              ZX-052 Buck IN- ──► OUT- ──────────► PCA9685 GND
    │
    └──[Fuse/Switch]──► ESP32 Expansion Board DC Jack (6.5-16V OK)
                         └── Onboard regulator → 5V & 3.3V for ESP32 logic
```

### Servo Channel Map (18-Servo Target)

#### Board 1 — `0x40` (Right Side, no solder pad change)

| Leg | Name | High (Coxa) | Mid (Femur) | Down (Tibia) |
|-----|------|-------------|-------------|--------------|
| 1 | Front Right | L1 (ch 0) | M1 (ch 1) | D1 (ch 2) |
| 2 | Mid Right | L2 (ch 3) | M2 (ch 4) | D2 (ch 5) |
| 3 | Rear Right | L3 (ch 6) | M3 (ch 7) | D3 (ch 8) |

#### Board 2 — `0x41` (Left Side, bridge A0 solder pad)

| Leg | Name | High (Coxa) | Mid (Femur) | Down (Tibia) |
|-----|------|-------------|-------------|--------------|
| 4 | Front Left | L4 (ch 0) | M4 (ch 1) | D4 (ch 2) |
| 5 | Mid Left | L5 (ch 3) | M5 (ch 4) | D5 (ch 5) |
| 6 | Rear Left | L6 (ch 6) | M6 (ch 7) | D6 (ch 8) |

### Current Test Setup (3 Servos Only)

```
ESP32 D21 (SDA) ─── PCA9685 (0x40) SDA
ESP32 D22 (SCL) ─── PCA9685 (0x40) SCL
ESP32 3V3       ─── PCA9685 (0x40) VCC
ESP32 GND       ─── PCA9685 (0x40) GND
ZX-052 6V OUT   ─── PCA9685 (0x40) V+

PCA9685 Channel 0 ─── 360° Servo (High - L1)
PCA9685 Channel 1 ─── 360° Servo (Mid - M1)
PCA9685 Channel 2 ─── 360° Servo (Down - D1)
```

---

## Power System

### Battery: Bonka 3S LiPo

| Parameter | Value |
|-----------|-------|
| Voltage | 11.1V nominal (12.6V full, 9.9V cutoff) |
| Capacity | 2200 mAh |
| Discharge | 35C continuous (77A), 70C burst (154A) |
| Connector | XT60 + JST-XH balance |
| Expected runtime | ~15–25 minutes active walking (full hexapod) |

**Future upgrade path:** 3S 4000–5000 mAh pack for longer runtime.

### Why LiPo (not Lead-Acid)?

- Lead-acid: Too heavy, low energy density, slow charging — unusable for legged robots.
- LiPo: High energy/kg, high discharge, compact, standard RC packs.

### Charger: iMAX B3 Pro AC

- 10W compact 2S/3S balance charger at ~800mA.
- **Known behavior:** Bottom case gets warm/hot during charging. This is normal for a fanless 10W unit.
- **Safety rules:**
  - Always charge on hard, non-flammable surface with airflow.
  - Monitor temperature; stop if uncomfortably hot or smells burnt.
  - **Upgrade path:** iMAX B6/B6AC if current charger seems insufficient.

### Buck Converter: ZX-052

- High-power adjustable DC-DC step-down module.
- IN+/IN-, OUT+/OUT- terminals, large inductor, voltage display.
- Rated approximately 15–20A max.
- **Must be set to exactly 6.0V** using multimeter + potentiometer (no load first, then verify under load).
- **Concern:** 18 MG996R servos can draw 10–20A during walking. If ZX-052 overheats or voltage sags:
  - **Fallback:** Use two ZX-052 modules, one per PCA9685 board (9 servos each), shared ground.

---

## Software & Firmware

### Project Structure (PlatformIO)

```
e:\hexa\
├── platformio.ini          ← PlatformIO config (ESP32 + Adafruit PCA9685)
├── src\
│   └── main.cpp            ← Current firmware: 3× 360° servo test & calibration
└── README.md               ← This file
```

### platformio.ini Config

```ini
[env:esp32dev]
platform  = espressif32
board     = esp32dev
framework = arduino
monitor_speed = 115200
upload_speed  = 115200

lib_deps =
    adafruit/Adafruit PWM Servo Driver Library @ ^2.4.1
    adafruit/Adafruit BusIO @ ^1.14.5
```

### Current Firmware: 360° Servo Test (`src/main.cpp`)

**Purpose:** Test and calibrate 3 continuous rotation servos before 180° servos arrive.

**How 360° servos work:**
- `1500 us` → STOP (motor holds still)
- `1500–2000 us` → Forward (higher = faster)
- `1500–1000 us` → Reverse (lower = faster)
- Each servo's exact stop point varies (~1480–1520 us)

**Serial Commands (115200 baud):**

| Command | Action | Example |
|---------|--------|---------|
| `R` | Run full demo sequence (fwd/stop/rev) | `R` |
| `X` | Emergency STOP all servos | `X` |
| `G [ch] [us]` | Set exact pulse on channel | `G 0 1520` |
| `F [ch]` | Full forward on channel | `F 1` |
| `B [ch]` | Full reverse on channel | `B 2` |
| `T [ch] [us]` | Tune stop-point calibration | `T 0 1495` |
| `P` | Print current calibration values | `P` |
| `?` | Print help menu | `?` |

### Uploading & Running

```bash
# Build and upload to ESP32 (hold BOOT button on ESP32 when "Connecting..." appears)
pio run --target upload

# Open Serial Monitor
pio device monitor
```

> **Upload tip:** If you get "Write timeout", hold the **BOOT button** on the ESP32 during the `Connecting...` phase. Also ensure you're using a **data USB cable** (not charge-only).

---

## Current Progress

### What's Working ✅

- [x] ESP32 boots and communicates over Serial (115200 baud)
- [x] PCA9685 board detected at 0x40 via I²C scan
- [x] 3× 360° servos respond to pulse commands
- [x] Serial command interface for speed control and calibration
- [x] PlatformIO project compiles and uploads successfully
- [x] ZX-052 buck converter providing 6V to servo V+

### What's Next 🔜

- [ ] Calibrate each 360° servo's exact stop-point (use `T` command)
- [x] Receive 180° MG996R servos → replace 360° servos
- [x] Flash full 18-servo firmware with IK + tripod gait
- [x] Wi-Fi/BLE remote control integration
- [ ] Bridge A0 pad on second PCA9685 board (set to 0x41)
- [ ] 3D print first leg → Phase 2 mechanical assembly
- [ ] Print full chassis → Phase 3
- [ ] Suspended gait testing → Phase 4
- [ ] Ground walking + thermal monitoring → Phase 5
- [ ] Wi-Fi/BLE remote control integration

---

## Known Issues & Solutions

| # | Problem | Solution | Status |
|---|---------|----------|--------|
| 1 | Arduino Mega + Uno + NRF24 too complex/expensive | Switch to ESP32 + PCA9685 + Wi-Fi/BLE | ✅ Resolved |
| 2 | Safe battery with enough current? | Bonka 3S 2200mAh 35C LiPo (77A continuous) | ✅ Resolved |
| 3 | Why not lead-acid battery? | Too heavy, low energy density for mobile robot | ✅ Resolved |
| 4 | iMAX B3 Pro charger getting hot | Normal for 10W fanless; charge on non-flammable surface | ✅ Monitored |
| 5 | Still need a buck with ESP32 expansion board? | Expansion board handles logic power; separate buck for servos only | ✅ Resolved |
| 6 | Is ZX-052 strong enough for 18 servos? | Likely yes (15-20A class); split to 2 bucks if overheats | ⏳ Test under load |
| 7 | PlatformIO upload "Write timeout" | Hold BOOT button on ESP32; use data cable; lower upload_speed to 115200 | ✅ Resolved |
| 8 | 360° servos creep at 1500us | Use `T` command to find exact stop-point per servo | ⏳ Calibrating |

---

## Build Phases

### Phase 1: Power & Logic Baseline ✅ *(Finished: 2026-04-30)*
- [x] ZX-052 output set to 6.0V
- [x] ESP32 expansion board boots from 3S LiPo
- [x] I²C scan detects PCA9685 at 0x40

### Phase 2: Single-Leg Subassembly & Calibration ✅ *(Finished: 2026-05-01)*
- [x] 3 servos connected to PCA9685 channels 0, 1, 2
- [x] Firmware uploaded, Serial commands working
- [x] Calibrate 360° servo stop-points
- [x] (When 180° servos arrive) Center all to 90°, attach horns
- [x] Print one leg, range-test 0–180°

### Phase 3: Full Chassis Assembly
- [ ] Calibrate remaining 15 servos
- [ ] Mount all servos to chassis
- [ ] Wire all 18 servos to both PCA9685 boards
- [ ] Route and tie down cables

### Phase 4: Suspended Kinematics Testing
- [ ] Hexapod on a stand, legs hanging freely
- [ ] Load IK + gait code
- [ ] Verify all legs move in correct directions
- [ ] Fix any mirrored/inverted movements

### Phase 5: Ground Walking & Thermal Test
- [ ] Place on floor, command slow tripod gait
- [ ] Monitor ZX-052 temperature under load
- [ ] If overheating → split power to 2 buck converters
- [ ] Tune gait speed and step parameters

---

## Information Log (Chronological)

All information provided by the project owner, timestamped for reference.

| Date & Time (IST) | Topic | Key Information |
|---|---|---|
| ~Mid-April 2026 | Initial concept | Reverse-engineer Emre Kalem's hexapod YouTube project; build cheaper and more modern |
| ~Mid-April 2026 | MCU decision | Replace Arduino Mega + Uno + NRF24L01 with ESP32 + PCA9685 + Wi-Fi/BLE |
| ~Mid-April 2026 | Servo choice | 18× MG996R servos (3 DOF × 6 legs) |
| ~Mid-April 2026 | Leg count debate | Explored quadruped (12 DOF) but decided to stay with hexapod (18 DOF) |
| ~Late April 2026 | Battery choice | Bonka 11.1V 2200mAh 35C 3S1P LiPo — enough current, compact |
| ~Late April 2026 | Chemistry rationale | LiPo over lead-acid: weight, energy density, discharge rate |
| ~Late April 2026 | Charger issue | iMAX B3 Pro AC bottom gets hot — normal for 10W fanless unit |
| ~Late April 2026 | ESP32 board | ESP32 30-pin DevKit on expansion board (6.5–16V DC input) |
| ~Late April 2026 | Power routing | Expansion board takes 3S directly for logic; ZX-052 buck for servos |
| ~Late April 2026 | Buck converter | ZX-052 adjustable DC-DC module, ~15–20A, set to 6.0V |
| ~Late April 2026 | 18-servo concern | ZX-052 borderline; fallback is 2 bucks split by side |
| 2026-04-24 14:30 IST | BOM spreadsheet | Created `hexapod_esp32_bom.xlsx` with pricing info |
| 2026-04-30 18:06 IST | Full project summary | Provided complete chronological summary of all decisions and hardware |
| 2026-04-30 18:06 IST | Build plan & servo map | Received 5-phase build plan + 18-servo channel mapping table |
| 2026-04-30 18:10 IST | Current servos clarified | Only 3× 360° continuous rotation servos available now (not 18× 180°) |
| 2026-04-30 18:10 IST | PlatformIO request | Switched from Arduino IDE (.ino) to PlatformIO project structure |
| 2026-04-30 18:24 IST | Upload attempt | First upload via `pio run --target upload` — "Write timeout" error |
| 2026-04-30 18:24 IST | Upload fix | Lowered upload_speed to 115200; hold BOOT button during connecting |
| 2026-04-30 ~18:30 IST | Upload success | Code uploaded successfully to ESP32 on COM4 |
| 2026-04-30 18:32 IST | Wiring confirmed | Wiring diagram reviewed; Serial Monitor running at 115200 baud |
| 2026-04-30 18:36 IST | Speed question | Asked about increasing 360° servo speed (can push to 500–2500us range) |
| 2026-04-30 20:01 IST | README request | This document — comprehensive project reference for future sessions |
| 2026-05-01 00:28 IST | Firmware Upgrade | Requested full 18-servo 180° IK firmware + Phone Control |

---

## 📐 Rules for AI Assistant

> **MANDATORY: Any AI assistant working on this project MUST follow these rules.**  
> **Read this entire README before writing any code or making any decisions.**

### Rule 1 — Read Before You Act
- **Always read this full README** at the start of every new chat session.
- Do NOT ask the user to re-explain things that are already documented here.
- Check the **Information Log**, **Known Issues**, and **Build Phases** before starting work.

### Rule 2 — Log Everything with Timestamps
- **Every time the user provides new information**, add a new row to the **Information Log (Chronological)** table at the bottom of this README.
- Format: `| YYYY-MM-DD HH:MM IST | Topic | Key Information |`
- Use the user's local time (IST, UTC+5:30).
- This includes: hardware changes, new parts received, test results, problems found, decisions made, design changes, anything relevant.

### Rule 3 — Update Progress as You Go
- When a task is completed, **check it off** in the **Current Progress** and **Build Phases** sections.
- When an entire Phase is completed, add the finished date to the header (e.g., `### Phase X ✅ *(Finished: YYYY-MM-DD)*`).
- When new tasks are identified, **add them** to the appropriate checklist.
- Update the `**Last Updated**` date at the top of the README, and move the old date into the `**Previous Updates**` list right below it. Never delete old update times.
- When the phase changes, do NOT delete the old `**Status**`. Instead, mark it as `**Phase X Status:** Finished on YYYY-MM-DD` and add the `**Current Status**` on a new line below it. Never delete history.

### Rule 4 — Respect Confirmed Decisions
These decisions are FINAL. Do NOT change them or suggest alternatives unless the user explicitly asks:

| Decision | Value | Status |
|----------|-------|--------|
| MCU | ESP32 30-pin DevKit (on expansion board) | ✅ Confirmed |
| Servo drivers | 2× PCA9685 (0x40 Right, 0x41 Left) | ✅ Confirmed |
| Servos (target) | 18× MG996R (180°), 3 DOF × 6 legs | ✅ Confirmed |
| Robot type | Hexapod (6 legs) | ✅ Confirmed |
| Battery | Bonka 3S 11.1V 2200mAh 35C LiPo | ✅ Confirmed |
| Servo buck converter | ZX-052, set to 6.0V | ✅ Confirmed |
| Logic power | 3S LiPo → ESP32 expansion board DC jack directly | ✅ Confirmed |
| Communication | Wi-Fi / BLE (built into ESP32) | ✅ Confirmed |
| Framework | PlatformIO (NOT Arduino IDE) | ✅ Confirmed |
| I²C pins | SDA = GPIO 21, SCL = GPIO 22 | ✅ Confirmed |
| Upload speed | 115200 baud | ✅ Confirmed |
| Serial monitor | 115200 baud | ✅ Confirmed |

### Rule 5 — Follow the Established Code Conventions
- **Language:** C++ with Arduino framework on PlatformIO.
- **Project root:** `e:\hexa\`
- **Source code:** `e:\hexa\src\main.cpp`
- **Config:** `e:\hexa\platformio.ini`
- **Library:** Adafruit PWM Servo Driver Library (via PlatformIO lib_deps).
- **All servo control** goes through PCA9685 `writeMicroseconds()` — never use direct GPIO PWM.
- **I²C bus** initialized with `Wire.begin(21, 22)` at 400kHz.
- Always include an **I²C scanner** in setup for diagnostics.
- Always include a **Serial command interface** for interactive testing.

### Rule 6 — Current Hardware State (Update This When Things Change)
| Item | Count | Type | Status |
|------|-------|------|--------|
| PCA9685 boards | 1 connected (0x40) | 16-channel servo driver | ✅ Working |
| PCA9685 boards | 1 available (needs A0 bridge for 0x41) | 16-channel servo driver | ⏳ Not yet wired |
| Servos connected | 3 | 360° continuous rotation | ✅ Working (need stop-point calibration) |
| MG996R 180° servos | 0 | Standard position servos | 📦 Not yet received |
| ESP32 | 1 | 30-pin DevKit on expansion board | ✅ Working on COM4 |
| ZX-052 Buck | 1 | Set to 6.0V | ✅ Working |
| 3S LiPo Battery | 1 | Bonka 2200mAh 35C | ✅ Working |

### Rule 7 — Safety Reminders (Include When Relevant)
- **360° servos** spin continuously — warn about fingers near joints.
- **LiPo battery** — never discharge below 9.9V (3.3V/cell), never charge unattended.
- **ZX-052** — monitor temperature under load; split to 2 bucks if it overheats.
- **Upload** — hold BOOT button on ESP32 during "Connecting..." phase.
- **Charger** — use on non-flammable surface only.

### Rule 8 — When User Gives New Info
When the user tells you something new about the project (new parts, test results, problems, design changes), you MUST:
1. **Acknowledge** the info in your response.
2. **Add a row** to the Information Log table with the current date/time (IST).
3. **Update** any affected sections (BOM, wiring, progress checklists, hardware state, known issues).
4. **Update** the `Last Updated` timestamp at the top.
5. If it's a new decision, add it to the **Confirmed Decisions** table in Rule 4.

---

## Reference Links

- **Inspiration:** Emre Kalem's 3D Printed RC Hexapod Spider Robot (YouTube)
- **PCA9685 Library:** [Adafruit PWM Servo Driver Library](https://github.com/adafruit/Adafruit-PWM-Servo-Driver-Library)
- **ESP32 Pinout:** SDA=GPIO21, SCL=GPIO22 (default I²C)
- **PlatformIO Docs:** [platformio.org](https://platformio.org/)
- **esptool Troubleshooting:** [espressif.com/esptool](https://docs.espressif.com/projects/esptool/en/latest/troubleshooting.html)

---

## How to Continue in a New Chat Session

Copy and paste this into any new AI chat to provide full context:

> **Project:** ESP32 Hexapod Spider Robot  
> **Repo:** `e:\hexa` (PlatformIO project)  
> **README:** `e:\hexa\README.md` — contains full project history, BOM, wiring, build phases, AND rules for the AI to follow.  
> **Current phase:** Phase 2 — 3× 360° servos on PCA9685 (0x40), calibrating stop-points.  
> **Next milestone:** Receive 180° MG996R servos → flash full IK firmware.  
> **⚠️ Read the ENTIRE README first — especially the "Rules for AI Assistant" section — before writing any code or making changes.**
