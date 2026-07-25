# RSSI-Based Human-Following Robot

A robot that follows a person by tracking WiFi signal strength (RSSI) from
their phone's hotspot, using 4 ESP32 receiver nodes for direction sensing
and 1 ESP32 master node for decision-making and motor control.

## How it works

1. The person's phone broadcasts a WiFi hotspot.
2. 4 ESP32 boards mounted at the robot's corners (front-left, front-right,
   back-left, back-right) each scan for that hotspot and measure RSSI
   (signal strength in dBm — closer to 0 is stronger, e.g. -40 is close,
   -85 is far).
3. Each receiver smooths its readings (raw RSSI is noisy) and sends them
   to a 5th "master" ESP32 over ESP-NOW (no router/WiFi AP needed for this
   link — it's direct ESP32-to-ESP32).
4. The master compares front vs back and left vs right averages to decide:
   move forward, turn left, turn right, turn around, or stop.
5. The master drives 2 DC motors through an L298N motor driver.

## Bill of materials

| Part | Qty | Notes |
|---|---|---|
| ESP32 dev board | 5 | 4 receivers + 1 master |
| L298N motor driver | 1 | or TB6612/BTS7960 — see note below |
| DC gear motors + wheels | 2 | differential drive |
| Caster wheel | 1 | for balance, front or back |
| Robot chassis | 1 | needs 4 mounting points for receiver spacing |
| Battery pack (motors) | 1 | e.g. 2S/3S Li-ion, matched to your motors |
| Battery/power bank (ESP32s) | 5 | or a shared regulated 5V rail |
| Jumper wires, breadboard/perfboard | - | |

If your motor driver isn't an L298N, only `master_node.ino`'s motor
control section (bottom of the file) needs to change — the RSSI/direction
logic above it stays the same.

## Setup steps

### 1. Get the master's MAC address
- Flash `get_mac_address.ino` to the board you'll use as master.
- Open Serial Monitor (115200 baud), copy the printed MAC address.

### 2. Configure and flash the 4 receivers
- Open `receiver_node.ino`.
- Set `TARGET_SSID` to your phone's hotspot name.
- Paste the master's MAC into `masterMAC[]`.
- Set `NODE_ID` to `0` for front-left, `1` for front-right, `2` for
  back-left, `3` for back-right — flash each board with its own ID.

### 3. Wire and flash the master
- Wire the L298N to the ESP32 as commented at the top of
  `master_node.ino` (ENA/IN1/IN2 = left motor, ENB/IN3/IN4 = right motor).
- Flash `master_node.ino` as-is (no per-board changes needed).

### 4. Calibrate before trusting the direction logic
- Power everything on, open the master's Serial Monitor.
- Walk around the robot at different distances and angles while watching
  the FL/FR/BL/BR values printed.
- Note your practical RSSI range (usually roughly -30 dBm close to -90
  dBm far, but this depends on your room/walls/phone).
- Adjust these constants in `master_node.ino` to match what you observed:
  - `STOP_RSSI_DBM` — value at which the robot should stop (person close)
  - `OUT_OF_RANGE_DBM` — value below which signal is considered lost
  - `TURN_THRESHOLD_DB` — how big a left/right difference should trigger
    a turn (too low = jittery/oscillating, too high = sluggish turning)

## Known limitations (mention these in your report/viva)

- RSSI is inherently noisy — multipath reflection, body blocking the
  phone, and phone orientation all cause fluctuation even at a fixed
  distance. The smoothing filter reduces but doesn't eliminate this.
- RSSI-based direction sensing is coarse compared to camera or UWB-based
  approaches — expect the robot to react in a "hunting" pattern rather
  than perfectly smooth tracking.
- Indoor environments with lots of walls/metal will compress your usable
  dBm range and reduce direction accuracy.
- This design has no obstacle avoidance — for a more complete project,
  add an ultrasonic/IR sensor on the master node and give it priority
  over the RSSI-based movement commands (e.g., stop/turn if an obstacle
  is under ~20cm regardless of what RSSI says).

## Suggested extensions (if you want to go further)

- Add a Kalman filter instead of the exponential moving average for
  smoother distance estimates.
- Fuse RSSI (distance) with an ultrasonic sensor (front obstacle) or a
  camera (person detection) for more reliable direction/tracking.
- Log RSSI vs. actual measured distance during calibration to build a
  rough distance-estimation curve instead of using raw dBm thresholds.
