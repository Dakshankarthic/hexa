# 🕷️ ESP32 Hexapod Spider Robot

> **Project Owner:** Dakshan  
> **Start Date:** April 2026  
> **Last Updated:** 2026-09-13 00:24 IST  
> **Previous Updates:** 2026-09-12 23:33 IST | 2026-09-12 20:20 IST | 2026-09-12 20:09 IST | 2026-09-12 20:08 IST | 2026-09-12 19:42 IST | 2026-09-12 19:36 IST | 2026-09-12 19:33 IST | 2026-09-12 19:26 IST | 2026-09-12 19:25 IST | 2026-09-12 19:22 IST | 2026-09-11 13:34 IST | 2026-09-11 13:21 IST | 2026-09-11 13:18 IST | 2026-09-11 13:13 IST | 2026-09-11 13:04 IST | 2026-09-11 12:58 IST | 2026-09-09 19:38 IST | 2026-09-09 19:30 IST | 2026-09-08 21:15 IST | 2026-09-08 20:44 IST | 2026-09-04 16:14 IST | 2026-09-04 16:10 IST | 2026-09-04 15:55 IST | 2026-09-04 15:49 IST | 2026-09-04 13:46 IST | 2026-09-04 13:27 IST | 2026-09-04 13:17 IST | 2026-09-04 13:11 IST | 2026-08-22 20:19 IST | 2026-08-22 20:17 IST | 2026-08-22 20:14 IST | 2026-08-22 19:59 IST | 2026-08-22 19:41 IST | 2026-05-28 18:46 IST | 2026-05-01 00:44 IST | 2026-05-01 00:30 IST | 2026-04-30 23:55 IST | 2026-04-30 20:00 IST
> **Phase 2 Status:** Finished on 2026-09-04 — Single-Leg Subassembly Testing Verified (L=Ch0, M=Ch1, D=Ch2 fully coordinated & dancing)
> **Current Status:** Phase 3 — Full 18-servo dual-PCA9685 controller (0x40 Right, 0x43 Left) with direction inversion (INV), trim calibration, gait choreography, and live SWAP command.

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

### PCA9685 Hardware Address Configuration (A0 Solder Bridge)

The PCA9685 module has 6 address selection pads along the upper edge of the PCB: `A0`, `A1`, `A2`, `A3`, `A4`, `A5`.
Each pad consists of two small exposed copper contacts separated by a narrow slit.

- **Base I²C Address:** `0x40` (binary `0b1000000`) when all pads are open / unbridged.
- **Address Formula:** $\text{Address} = 0\text{x}40 + (A_5 \cdot 32 + A_4 \cdot 16 + A_3 \cdot 8 + A_2 \cdot 4 + A_1 \cdot 2 + A_0 \cdot 1)$
- **Board 1 (Right Side — 0x40):** Leave all pads untouched (open).
- **Board 2 (Left Side — 0x41):** Bridge the two halves of the **`A0`** solder pad with a small drop of solder:
  - `A0` bridged = $+1 \implies 0\text{x}40 + 0\text{x}01 = \mathbf{0\text{x}41}$.
  - Leave `A1`, `A2`, `A3`, `A4`, `A5` untouched / open.

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

### Servo Channel Map (18-Servo — Confirmed 2026-09-08)

> ⚠️ Joint order per leg is **D, M, L** (Tibia first, Coxa last on each 3-channel group).

#### Board 1 — `0x40` Main (Right Side: Legs 1, 2, 3)

| Channel | Servo | Leg |
|---------|-------|-----|
| ch 0 | D1 (Tibia) | Leg 1 — Front Right |
| ch 1 | M1 (Femur) | Leg 1 — Front Right |
| ch 2 | L1 (Coxa) | Leg 1 — Front Right |
| ch 3 | D2 (Tibia) | Leg 2 — Mid Right |
| ch 4 | M2 (Femur) | Leg 2 — Mid Right |
| ch 5 | L2 (Coxa) | Leg 2 — Mid Right |
| ch 6 | D3 (Tibia) | Leg 3 — Rear Right |
| ch 7 | M3 (Femur) | Leg 3 — Rear Right |
| ch 8 | L3 (Coxa) | Leg 3 — Rear Right |

#### Board 2 — `0x43` Aux (Left Side: Legs 6, 5, 4 — A0+A1 bridged)

| Channel | Servo | Leg |
|---------|-------|-----|
| ch 0 | D6 (Tibia) | Leg 6 — Rear Left |
| ch 1 | M6 (Femur) | Leg 6 — Rear Left |
| ch 2 | L6 (Coxa) | Leg 6 — Rear Left |
| ch 3 | D5 (Tibia) | Leg 5 — Mid Left |
| ch 4 | M5 (Femur) | Leg 5 — Mid Left |
| ch 5 | L5 (Coxa) | Leg 5 — Mid Left |
| ch 6 | D4 (Tibia) | Leg 4 — Front Left |
| ch 7 | M4 (Femur) | Leg 4 — Front Left |
| ch 8 | L4 (Coxa) | Leg 4 — Front Left |

### Physical Kinematics & Leg Layout (Confirmed 2026-09-08)

#### Top View — Leg Layout & Gait Direction

```
                 ▲ FRONT (Forward Gait Direction)
          Leg 4 ╲       ╱ Leg 1 (Front Right)
                 ╲ ─── ╱
   (Mid Left) Leg 5 │   │ Leg 2 (Mid Right)
                 ╱ ─── ╲
          Leg 6 ╱       ╲ Leg 3 (Rear Right)
                 ▼ REAR
```

- **Tripod Group A:** Legs 1, 3, 5 (Front Right, Rear Right, Mid Left)
- **Tripod Group B:** Legs 2, 4, 6 (Mid Right, Front Left, Rear Left)
- **Symmetry:** Left-side coxa swing is mirrored relative to right-side for forward locomotion.

#### Side View — 3-DOF Joint Motion Axes

```
  Hexapod Chassis ──[L: Coxa (Yaw)]──┬──[M: Femur (Pitch)]──┬──[D: Tibia (Pitch)]── Foot Contact
                                     │                      │
                           (Horizontal Swing)       (Vertical Lift)      (Ground Contact)
```

| Joint | Anatomical Name | Motion Axis | Function | Default Pose |
|-------|-----------------|-------------|----------|--------------|
| **L** | Coxa (Hip) | Horizontal (Yaw) | Swings leg forward & backward for walking | 90° (Neutral) |
| **M** | Femur (Thigh) | Vertical (Pitch) | Lifts leg off ground during swing phase | 60° (Stand) / 90° (Center) |
| **D** | Tibia (Shin/Foot) | Vertical (Pitch) | Extends down to push or tucks during step | 120° (Stand) / 90° (Center) |

### Dual PCA9685 I²C Daisy-Chain Bus

```
ESP32 D21 (SDA) ──┬── PCA9685 Main (0x40) SDA ──┬── PCA9685 Aux (0x41) SDA
ESP32 D22 (SCL) ──┼── PCA9685 Main (0x40) SCL ──┼── PCA9685 Aux (0x41) SCL
ESP32 3V3       ──┼── PCA9685 Main (0x40) VCC ──┼── PCA9685 Aux (0x41) VCC
ESP32 GND       ──┼── PCA9685 Main (0x40) GND ──┼── PCA9685 Aux (0x41) GND
ZX-052 6V OUT   ──┴── PCA9685 Main (0x40) V+  ──┴── PCA9685 Aux (0x41) V+
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
d:\hexa\
├── platformio.ini          ← PlatformIO config (ESP32 + Adafruit PCA9685 + BT)
├── src\
│   └── main.cpp            ← Full 18-servo dual-PCA9685 controller firmware
├── rule_book_dk.md         ← This file (full project reference)
└── README.md               ← Quick readme
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
- [x] Single 3-DOF leg (L=Ch0, M=Ch1, D=Ch2) verified with coordinated dance & step motion
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
| 2026-05-28 18:46 IST | 180° Servos & Weight Calibration | Confirmed they are using 180° servos now. Reported that they calibrated after attaching, but under the robot's weight the legs sag (go down), losing calibration. Asked how to resolve. |
| 2026-08-22 19:41 IST | L-Series Motor Testing | Requested dedicated firmware to test and run only the L-series (Coxa/Hip) motors L1 to L6 across both PCA9685 boards (0x40 & 0x41). |
| 2026-08-22 19:59 IST | Single Board L-Series Wiring | Configured all L-series servos (L1 to L6) onto Board 1 (0x40) channels 0 to 5 for single PCA9685 testing. |
| 2026-08-22 20:14 IST | L-Series Upload & Operation | Firmware uploaded successfully to ESP32. User guided on serial/Bluetooth operation commands. |
| 2026-08-22 20:17 IST | I2C Troubleshooting | Serial Monitor showed PCA9685 0x40 not detected over I2C (SDA=21, SCL=22). Provided wiring diagnostics (VCC, GND, SDA/SCL pin swap). |
| 2026-08-22 20:19 IST | PCA9685 Soldering & I2C Clarification | Clarified that no address pad soldering is needed for Board 1 (default 0x40); confirmed SDA=GPIO21 and SCL=GPIO22. |
| 2026-09-04 13:11 IST | Single-Leg (L, M, D) Control | User connected single 3-DOF leg: L in Ch 1, M in Ch 2, D in Ch 3 on PCA9685 (0x40). Flashed dedicated firmware with manual angles, stand pose, and walking step-cycle gait. |
| 2026-09-04 13:17 IST | Single-Leg Dance Routine | Choreographed dynamic 4-phase rhythm dance (Hip Sway & Knee Bounce, Rapid Toe Tap, Can-Can High Kick, Snake Body Wave) with auto-start on boot. |
| 2026-09-04 13:27 IST | Exact Channel Map Confirmed | Clarified and confirmed exact 0-indexed PCA9685 pinout: L = Channel 0, M = Channel 1, D = Channel 2. Recompiled and validated. |
| 2026-09-04 13:46 IST | Single-Leg Dance Confirmed Working | User confirmed all 3 joints (L, M, D) are dancing and moving in full synchronization. Phase 2 single-leg kinematics baseline successfully verified. |
| 2026-09-04 15:49 IST | Servo 90° Horn Alignment | User reported horn attachment was not set precisely at 90°. Outlined hardware re-seating method and provided software trim offset calibration. |
| 2026-09-04 15:55 IST | 300W 20A Buck Converter Setup | User acquired 300W 20A step-down buck module. Provided exact CV (6.0V) and CC (max headroom) potentiometer tuning instructions. |
| 2026-09-04 16:10 IST | Resistance & CC/CV Verification | Clarified multimeter measurement rules (never measure resistance on live board; verify CV via DC volts and CC via slip-clutch click / load testing). |
| 2026-09-04 16:14 IST | Potentiometer Model Confirmed | User confirmed trimpots are W503 (50kΩ, 25-turn precision 3296W potentiometers). Explained resistance decoding and adjustment mechanics. |
| 2026-09-08 20:44 IST | Full 18-Servo Channel Map Confirmed | User provided handwritten diagram confirming new dual-board wiring: 0x40(Main)=Legs 6,5,4 and 0x41(Aux)=Legs 1,2,3 with D,M,L order per leg group. Firmware rewritten from single-leg test to full 18-servo dual-PCA9685 controller with tripod gait walk, dance, wave, sweep, per-joint serial/BT commands, TRIM calibration, RELAX, SCAN diagnostics. Boot centers all servos to 90°. |
| 2026-09-08 21:15 IST | Robot Kinematic Geometry & Leg Direction Diagrams | User provided top & side diagram sketches specifying the physical orientation of all 6 legs around hexagonal chassis and 3-DOF joint motion axes (L = horizontal Coxa yaw swing, M = vertical Femur pitch lift, D = vertical Tibia pitch extension). Left vs right leg mirroring confirmed. Added software direction inversion (`INV` command) to `main.cpp` for instant per-joint direction flipping without reflashing. Verified zero-error compilation with PlatformIO. |
| 2026-09-09 19:30 IST | Set All Legs to 75° (Calibration/Alignment) | User requested moving all legs to 75°. Live command `ALL 75` moves all 18 servos across all 6 legs on dual PCA9685 boards to 75.0° holding pose via USB Serial or Bluetooth. Also documented `SET 75 75 75` for per-joint triplet control. |
| 2026-09-09 19:38 IST | Swapped PCA Board IDs | User requested swapping PCA board IDs. Tested assigning Board 0x40 to Right legs and Board 0x41 to Left legs. |
| 2026-09-11 12:58 IST | Swapped PCA Bus IDs | User requested swapping PCA bus IDs. Assigned Board 0x40 (Main) to Left legs (Legs 4, 5, 6) and Board 0x41 (Aux) to Right legs (Legs 1, 2, 3) to match hardware connection state. |
| 2026-09-11 13:04 IST | Swapped PCA Board Mapping (0x40 Right, 0x41 Left) | User requested swapping back again after hardware test. Board 0x40 (Main) is assigned to Right legs (Legs 1, 2, 3) and Board 0x41 (Aux) to Left legs (Legs 4, 5, 6). |
| 2026-09-11 13:13 IST | Servo Pulse Driver Fix & All-Channel Broadcast | User reported servos not moving to 90°. Replaced vulnerable `writeMicroseconds()` (which performed unreliable I2C prescale reads that could shut off PWM) with direct, deterministic `setPWM()` 12-bit tick calculations (307 ticks = 1500µs = 90°). Added broadcast of 90° center pulse to channels 0–15 on boot and in `ALL` command. Lowered I2C clock to 100kHz for maximum noise immunity. Verified build. |
| 2026-09-11 13:18 IST | Hardware & Power Diagnostics for Servos | Troubleshooting zero servo response: verified software is transmitting 307 ticks (1500µs) on channels 0-15; guided user to check V+ screw terminal power, green LED on PCA9685, servo connector polarity (signal vs GND), and second board connection. |
| 2026-09-11 13:21 IST | Swapped PCA IDs & Restored 400kHz I2C | User requested swapping PCA IDs. Assigned Board 0x40 to Left (Legs 4, 5, 6) and Board 0x41 to Right (Legs 1, 2, 3). Restored 400kHz I2C clock. |
| 2026-09-11 13:34 IST | Swapped PCA Board IDs (0x40 Right, 0x41 Left) | User requested swapping PCA IDs. Swapped assignments in firmware: Board 0x40 (Main) assigned to Right side (Legs 1, 2, 3) and Board 0x41 (Aux) assigned to Left side (Legs 4, 5, 6). Verified compilation cleanly passes with PlatformIO. |
| 2026-09-12 19:22 IST | PCA9685 0x41 Hardware Address Setup | User asked how to set the second PCA9685 board to address 0x41. Explained A0 solder bridge location, binary offset formula (0x40 + 1), step-by-step soldering instructions, daisy-chain I2C bus wiring (SDA, SCL, VCC, GND), and V+ power routing. |
| 2026-09-12 19:25 IST | Second PCA9685 Board Already Soldered | User confirmed that one PCA9685 board already has its address jumper soldered. That soldered board is configured as 0x41 (Left Side: Legs 4, 5, 6), while the unsoldered board is 0x40 (Right Side: Legs 1, 2, 3). Guided user on connecting the daisy-chain jumpers to test detection. |
| 2026-09-12 19:26 IST | Address Pad Status on All PCA Boards | User noted that of the 6 pads, 1 pad appears soldered on all PCA boards. Outlined how to verify actual I2C address via single-board SCAN, how HASL solder coating differs from a bridge, and how to bridge A1 (+2) for address 0x43 or 0x42 if both boards share the same base address. |
| 2026-09-12 19:33 IST | Live I2C Scan Diagnostic (0x40 Only) | User executed SCAN in serial monitor; only 0x40 was found. Corrected display tags in scanI2C() (0x40 Right, 0x41 Left). Outlined 3-step diagnostic to test Board 2 independently to verify if it is unpowered, miswired, or also on 0x40. |
| 2026-09-12 19:36 IST | I2C Full-Range Bus Scan Clarification | User asked whether SCAN checks for specific IDs or all IDs. Confirmed scanI2C() tests all 112 valid 7-bit addresses (0x08 through 0x77); finding only 0x40 confirms no other device responded anywhere on the bus. |
| 2026-09-12 19:42 IST | Hardware Address Confirmed: Board 2 is 0x43 | Live I2C scan confirmed Board 2 responds at address 0x43 (A0 + A1 bridged: 0x40 + 1 + 2 = 0x43) along with 0x70 (PCA9685 All-Call broadcast). Firmware updated with PCA_AUX_ADDR=0x43 (Left Side: Legs 4, 5, 6) and PCA_MAIN_ADDR=0x40 (Right Side: Legs 1, 2, 3). Recompiled cleanly. |
| 2026-09-12 20:08 IST | Dual-Board Bus Drop Troubleshooting | User reported that each board responds individually (0x40 and 0x43), but when connecting both, only one responds (or vice-versa). Diagnosed header pinout traps (OE pin offset between GND and SCL), star-wiring method, and shared ground requirements. |
| 2026-09-12 20:09 IST | Daisy-Chain Pass-Through Diagnosis | Identified that whichever board is connected directly to the ESP32 is detected (0x40 or 0x43), while the daisy-chained second board is unpowered due to the OE pin offset on the 6-pin header. Instructed user to connect both boards directly to the ESP32 in parallel. |
| 2026-09-12 20:20 IST | Both PCA9685 Boards Simultaneously Online | User verified both boards detected simultaneously: Board 1 at 0x40 (Right: Legs 1-3) and Board 2 at 0x43 (Left: Legs 4-6). Also detected Sub-Call addresses (0x71, 0x72, 0x74) and All-Call (0x70). Dual-PCA9685 hardware I2C bus successfully verified. |
| 2026-09-12 23:33 IST | Pushed to GitHub Repository | Committed and pushed changes to origin/main (commit 446deac): dual-PCA9685 controller (0x40 Right, 0x43 Left), direct PWM ticks, updated rule book, and Android bluetooth gamepad controller app. Working tree clean. |
| 2026-09-13 00:05 IST | Communication Protocols Review | Documented all protocols used: I²C (400 kHz, GPIO 21/22) for PCA9685 servo drivers, PWM (50 Hz, 500–2500 µs) for MG996R servos, Bluetooth Classic SPP ("HEXA-SPIDER") for phone control, and UART (115200 baud) for USB Serial debug. Discussed Bluetooth latency (~20–100 ms) vs alternatives: ESP-NOW (~1–5 ms, native), BLE (~7.5–15 ms, native), WiFi UDP (~5–10 ms, native). Zigbee not viable on ESP32 DevKit (needs ESP32-C6/H2). |
| 2026-09-13 00:24 IST | 90° Center Pose Diagram Created | Generated accurate technical diagram showing hexapod with all 18 servos at 90° center position. Includes both TOP VIEW (hexagonal body, 6 legs radiating perpendicular) and SIDE VIEW (single leg showing Coxa→Femur→Tibia all perfectly horizontal in one straight line). At 90° center the robot cannot stand — legs stick straight out flat. Diagram saved to `docs/hexapod_90deg_center_pose.jpg`. Compare with STAND pose (L=90, M=60, D=120) which angles legs down to support weight. |

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
| Servo drivers | 2× PCA9685 (0x40 Right Legs 1,2,3; 0x43 Left Legs 4,5,6) | ✅ Confirmed (Updated 2026-09-12 19:42) |
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
| Channel order per leg | D, M, L (Tibia first, Coxa last) | ✅ Confirmed (2026-09-08) |
| Board 0x40 legs | Legs 1, 2, 3 (Right Side) | ✅ Confirmed (Updated 2026-09-12 19:42) |
| Board 0x43 legs | Legs 4, 5, 6 (Left Side) | ✅ Confirmed (Updated 2026-09-12 19:42) |
| Joint kinematic axes | L (Coxa Yaw swing), M (Femur Pitch lift), D (Tibia Pitch extend) | ✅ Confirmed (2026-09-08) |

### Rule 5 — Follow the Established Code Conventions
- **Language:** C++ with Arduino framework on PlatformIO.
- **Project root:** `d:\hexa\`
- **Source code:** `d:\hexa\src\main.cpp`
- **Config:** `d:\hexa\platformio.ini`
- **Library:** Adafruit PWM Servo Driver Library (via PlatformIO lib_deps).
- **All servo control** goes through PCA9685 `writeMicroseconds()` — never use direct GPIO PWM.
- **I²C bus** initialized with `Wire.begin(21, 22)` at 400kHz.
- Always include an **I²C scanner** in setup for diagnostics.
- Always include a **Serial command interface** for interactive testing.

### Rule 6 — Current Hardware State (Update This When Things Change)
| Item | Count | Type | Status |
|------|-------|------|--------|
| PCA9685 boards | 2 connected (0x40 & 0x43) | 16-channel servo driver | ✅ Both online & fully operational |
| Servos connected | 18 | 180° standard positional (MG996R) | ✅ Working (experiencing leg sag under load) |
| MG996R 180° servos | 18 | Standard position servos | ✅ Received and installed |
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
> **Repo:** `d:\hexa` (PlatformIO project)  
> **README:** `d:\hexa\rule_book_dk.md` — contains full project history, BOM, wiring, build phases, AND rules for the AI to follow.  
> **Current phase:** Phase 3 — Full 18-servo dual-PCA9685 controller (0x40 Left, 0x41 Right) with direction inversion (INV), trim calibration, gait choreography, and live board swap (SWAP).  
> **Next milestone:** Calibration and gait tuning with all 18 servos mounted.  
> **⚠️ Read the ENTIRE rule_book_dk.md first — especially the "Rules for AI Assistant" section — before writing any code or making changes.**
