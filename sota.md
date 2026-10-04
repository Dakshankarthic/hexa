# 🧠 SOTA — Stand & Walk Logic (Emre Kalem Hexapod v1.0.0)

> **Created:** 2026-10-04 22:53 IST
> **Source:** Emre Kalem & Özgür Toprak Önsoy, *Hexapod source code v1.0.0* (MIT License, 2025)
> **Reference copy:** [`docs/reference/Hexapod_Arduino.ino`](docs/reference/Hexapod_Arduino.ino) (not compiled)
> **Purpose:** The working reference ("state of the art") for how the original robot **stands** and **walks**, so we can port it to our ESP32 + dual PCA9685 build.

---

## 1. The Big Idea (read this first)

Emre's code **never moves servos by hand-picked angle offsets.**
It works like this:

1. Choose **where each foot should be in space** (x, y, z in mm).
2. Run **Inverse Kinematics (IK)** to work out the 3 servo angles.
3. Write those angles to the servos.

**Stand** = every foot sits at its fixed "home" point.
**Walk** = every foot follows a moving path around its home point.

Our current `main.cpp` adds fixed angle offsets to a stand pose (e.g. `M+lift`, `L±swing`).
That's the main difference, and it's why our walk can drag or tip the robot (see §8).

---

## 2. Robot Geometry

### Link lengths (mm)
| Link | Joint (our name) | Length |
|------|------------------|--------|
| Coxa | **L** (hip yaw) | 38.0 |
| Femur | **M** (thigh pitch) | 86.25 |
| Tibia | **D** (knee/shin pitch) | 160.5 |

### Body frame
- **+X** = forward, **+Y** = left, **+Z** = up. Origin = body centre.
- `origin.z` stores each leg's **mount yaw** = `atan2(y, x)`.

| Index | Leg | Hip mount (x, y) mm | Mount yaw | Tripod group |
|-------|-----|---------------------|-----------|--------------|
| 0 | RF (Right Front) | (100.94, −57.11) | −29.5° | **A** |
| 1 | RM (Right Mid) | (0, −105.72) | −90° | **B** |
| 2 | RR (Right Rear) | (−100.94, −57.11) | −150.5° | **A** |
| 3 | LR (Left Rear) | (−100.94, 57.11) | 150.5° | **B** |
| 4 | LM (Left Mid) | (0, 105.72) | 90° | **A** |
| 5 | LF (Left Front) | (100.94, 57.11) | 29.5° | **B** |

### Tunable constants
| Constant | Value | Meaning |
|----------|-------|---------|
| `STEP_LENGTH` | 60 mm | Full fore/aft foot stroke (±30 mm around home) |
| `STEP_HEIGHT` | 100 mm | Peak foot lift during swing |
| `DEFAULT_Z` | −40 mm | Foot height relative to the hip plane (foot is 40 mm *below* the hip) |
| `ROTATION_STEP_RAD` | 15° | Full turn stroke per step (±7.5°) |
| `WALK/STOP_SMOOTHING_FACTOR` | 0.4 | Low-pass filter on foot targets |
| `t += 0.015` | per loop | 1 gait cycle ≈ 67 loops ≈ **0.7–0.8 s** (with `delay(10)`) |

---

## 3. STAND Logic (step by step)

```
setup():
  for each leg:
     init()            → origin.z = atan2(y,x); all 3 servos write(90)
     FK(neutralAngles) → neutralAngles = (coxa 0°, femur +45°, tibia −90°)
     home   = FK foot point
     home.z = DEFAULT_Z (−40)      ← overrides FK z (−52.5) to set body height
     target = home
  delay(1000)

loop() with joystick centred:
  get_foot_target() returns home → smoothing pulls target to home → IK → servos
```

### What "home" actually is
- FK with femur 45° up and tibia 90° bent gives a horizontal reach of
  `38 + 86.25·cos45° + 160.5·cos45° = 212.48 mm` from the hip, along the leg's mount direction.
- Then z is forced to **−40 mm**, so the **body sits 40 mm above the feet** (hip plane to foot).
- Every leg's home is the same in its *own* frame: **212.48 mm out, 40 mm down**.

### Stand servo angles that come out of the IK (same for all 6 legs)
| Joint | IK angle | Servo write | Meaning |
|-------|----------|-------------|---------|
| Coxa (L) | 0° | **90.0°** | Leg points straight out from the body |
| Femur (M) | +50.7° | **140.7°** | Thigh raised 50.7° above horizontal |
| Tibia (D) | −92.4° | **92.4°** | Shin bent ~92° from the thigh line (about vertical) |

> ⚠️ **Your robot stands at L=90, M=90, D=175** — a different servo zero/direction convention.
> **Do NOT copy Emre's raw numbers.** Use the **delta mapping** in §7.

---

## 4. WALK Logic (step by step)

### 4.1 Input → command
```
radioData[0] < 200 → direction = −1 (backward)   > 800 → +1 (forward)
radioData[3] < 200 → rotation  = +1 (turn left/CCW)  > 800 → −1 (turn right/CW)
else 0                     (on/off only — no proportional speed)
```

### 4.2 Gait clock & tripod phase
```
t += 0.015 every loop
phase = t mod 1
if leg index is odd (RM, LR, LF): phase = (phase + 0.5) mod 1
```
- **Tripod A** = RF, RR, LM (legs 0, 2, 4)
- **Tripod B** = RM, LR, LF (legs 1, 3, 5)
- They are always exactly **half a cycle apart** → 3 feet on the ground at all times.

### 4.3 Foot trajectory per phase (`get_foot_target`)
| Phase | Name | Fore/aft (`step_x`) | Turn (`step_rot`) | Lift (`step_z`) |
|-------|------|----------------------|-------------------|-----------------|
| 0.0 → 0.5 | **STANCE** (foot on ground, pushes) | `+30 → −30 mm` × direction (linear) | `+7.5° → −7.5°` × rotation | **0** |
| 0.5 → 1.0 | **SWING** (foot in air, returns) | `−30 → +30 mm` × direction (linear) | `−7.5° → +7.5°` × rotation | `sin(π·p) × 100 mm` (half-sine arc) |

Final target:
```
rotated_home = rotate(home, step_rot) about body centre (Z axis)
target.x = rotated_home.x + step_x
target.y = rotated_home.y
target.z = home.z + step_z
```
- **Forward walk:** each grounded foot slides **backward** in a straight line under the body → body moves forward.
- **Turn on the spot:** each foot traces an **arc around the body centre** → body yaws. Walk and turn can be mixed.
- **Stop:** if `direction == 0 && rotation == 0`, the target is home **immediately** (all legs settle to home through the smoothing filter).

### 4.4 Smoothing (low-pass filter)
```
target += (goal − target) × 0.4      // every loop, every axis
```
- Removes jerks when you start, stop or change direction.
- Walk and stop both use 0.4, so the ternary does nothing (and it only checks `direction`, not `rotation`).

### 4.5 Write to servos
```
IK(target) → (yaw, femur, knee)
coxa  = constrain(90 + yaw, 45, 135)
femur = 90 + femur
tibia = |knee|
```
No per-side inversion in software. Left/right mirroring is handled by how the servos are physically mounted.

---

## 5. IK Math (`calculate_ik`)

```
1. rel = target − hip_origin                    (x, y);  z stays as is
2. rotate rel by −mount_yaw → leg-local frame   (local +x = straight out from body)
3. yaw      = atan2(rel.y, rel.x)               → coxa
4. l_xy     = √(x² + y²);   l_fwd = l_xy − COXA
5. D        = √(l_fwd² + z²)   clamp to [|F−T|, F+T]  (reach safety)
6. knee     = acos((F² + T² − D²) / 2FT) − π    → tibia (0 = straight, −90 = right angle)
7. β1 = atan2(z, l_fwd);  β2 = acos((F² + D² − T²) / 2FD)
   femur    = β1 + β2                           → femur (0 = horizontal, + = up)
```
This is the standard **law of cosines 2-link planar IK** plus a coxa yaw.

### FK (`calculate_fk`) — used only once, to build `home`
```
p1 = hip + COXA·(cos α, sin α, 0)                       α = coxa + mount_yaw
p3 = p1 + (F·cosβ + T·cos(β+γ))·(cos α, sin α)  ,  z = F·sinβ + T·sin(β+γ)
```

---

## 6. Worked Numbers (verified by script)

| Situation | Leg | L (coxa) | M (femur) | D (tibia) |
|-----------|-----|----------|-----------|-----------|
| **Stand / home** | all | 90.0 | 140.7 | 92.4 |
| Swing peak (+100 mm) | all | 90.0 | **169.4** | 88.3 |
| Stance front end | RM (side) | 98.0 | 139.7 | 90.9 |
| Stance back end | RM (side) | 82.0 | 139.7 | 90.9 |
| Stance front end | RF (corner) | 93.5 | **126.7** | **71.4** |
| Stance back end | RF (corner) | 85.5 | **153.1** | **109.9** |

### What this tells us
1. **Lifting is mostly the femur (M):** +28.7° on M, the tibia barely moves (−4°).
2. **Middle legs push with the coxa (L) only** (±8°). M and D hardly change, because the foot slides sideways to the leg.
3. **Corner legs use ALL 3 joints in stance.** M swings ±13° and D ±19° to keep the foot **flat on the ground in a straight line**, because the forward slide is partly *along* the leg (it changes reach).
4. Foot height stays exactly −40 mm during stance on every leg, so the body stays level.

➡️ A gait that only swings L and keeps M/D fixed (our current approach) makes the **corner feet move in an arc and change height**. That means drag, scuffing, body tilt and tipping.

---

## 7. Porting to Our Robot (ESP32 + PCA9685, stand L=90 M=90 D=175)

### 7.1 Delta mapping (keep our confirmed stand pose)
Use Emre's IK only for **how much each joint changes from stand**, then add that change to **our** stand angles:

```cpp
// Emre IK reference at home
const float E_L0 = 90.0f, E_M0 = 140.7f, E_D0 = 92.4f;
// Our confirmed stand (2026-10-04)
const float U_L0 = 90.0f, U_M0 = 90.0f,  U_D0 = 175.0f;
// Direction signs — MUST be found by test (+1 or −1 per joint, per side)
float sL, sM, sD;

ourL = U_L0 + sL * (emreL - E_L0);
ourM = U_M0 + sM * (emreM - E_M0);
ourD = U_D0 + sD * (emreD - E_D0);
```

### 7.2 Things to check before porting
| # | Check | Why |
|---|-------|-----|
| 1 | Measure our real coxa/femur/tibia lengths | IK only works with correct link lengths (assumed same 3D-printed parts: 38 / 86.25 / 160.5) |
| 2 | Find `sM`, `sD` per side with the robot on a stand | Left and right are mirrored; our `INV` flags already handle part of this |
| 3 | **D=175 leaves only 5° headroom** before 180° | Corner legs need D to change by about ±19°. If `sD = +1`, clamp at 180 or re-trim D |
| 4 | Lower `STEP_HEIGHT` to 40–60 mm first | 100 mm needs M = +29°; start smaller on our heavier ESP32/battery build |
| 5 | Loop timing: `t += 0.015` per ~11 ms | Match it on ESP32 with `millis()`-based ticks (≈ 1.5 cycles/s) |
| 6 | Map RF/RM/RR/LR/LM/LF → our Legs 1–6 and PCA channels (D, M, L order) | Our channel map differs from Emre's Mega pins |
| 7 | Keep coxa constrained 45–135 | Stops legs from hitting each other |

### 7.3 Recommended port order
1. Add `Vector`, FK, IK and `get_foot_target` to `main.cpp` (pure math, no hardware change).
2. Add the delta mapping + per-joint sign table.
3. Test **IK-stand** first: it must output exactly 90/90/175 on all legs.
4. Robot on a stand → `WALK` at STEP_HEIGHT 40 mm → check each foot goes up and moves the right way.
5. On the ground → raise STEP_HEIGHT / speed step by step.

---

## 8. Our Current Gait vs SOTA — Comparison

| Feature | Our `main.cpp` (2026-10-04) | Emre SOTA |
|---------|-----------------------------|-----------|
| Control space | Joint angle offsets | **Foot XYZ (Cartesian) + IK** |
| Stand pose | Hand-tuned 90/90/175 ✅ | IK from foot point (212 mm out, 40 mm down) |
| Stance push | Coxa sweep only | Straight-line foot slide (all joints on corner legs) |
| Foot height in stance | Changes on corner legs ❌ | Constant −40 mm ✅ |
| Lift | M + lift, D − tuck | Half-sine 100 mm arc (mostly femur) |
| Gaits | Ripple + Tripod | Tripod only |
| Turning | Coxa-only offsets | Arc around body centre |
| Smoothing | — | Exponential filter α = 0.4 |
| Speed control | Fixed period | Fixed (on/off joystick) |

---

## 9. Known Quirks in the Reference Code
- `servoPins({pin0, pin1, pin2})` array init in a constructor initializer list is non-standard C++ (works on AVR GCC).
- The smoothing factor ternary is a no-op (both 0.4) and ignores `rotation`.
- Stop is instant-to-home: legs that are in the air drop straight down instead of finishing their step.
- `radioData` is read with no timeout, so if the radio link drops, the last command keeps running.
- `t` grows forever (float precision drops after hours). Wrap it with `fmod`.

---

*Credit: Original algorithm © 2025 Emre Kalem (MIT License). This document is an analysis for the d:\hexa ESP32 port.*
