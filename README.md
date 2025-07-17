# RoboCan M5Stack Stepper Control Documentation

This firmware runs two TB6600 stepper drivers from an Xbox BLE controller on an M5Stack Basic. You can live-drive, record micro-movement segments, save/load them from flash, and replay at the same speed.

---

## 1. Hardware & Pin Mapping

- **M5Stack Basic (ESP32)**  
- **TB6600 #1**  
  - ENA– → GPIO 5  
  - DIR– → GPIO 17  
  - PUL– → GPIO 16  
- **TB6600 #2**  
  - ENA– → Grove A pin 2 (GPIO 21)  
  - DIR– → Grove A pin 3 (GPIO 22)  
  - PUL– → Grove B pin 2 (GPIO 26)  
- All grounds tied together  
- Step pulse: 200 µs HIGH, 200 µs LOW

---

## 2. Firmware Overview

1. **Initialization**  
   - Initialize M5Stack display and serial port  
   - Start BLE-HID host for Xbox controller  
   - Configure stepper pins and disable drivers  
   - Load saved segments from flash  

2. **Main Loop**  
   - Call `xbox.onLoop()` to process BLE events  
   - Show a “CONNECTED!” splash on first pairing  
   - Read button edges and thumbstick values  
   - Handle record controls (LB / RB)  
   - Handle playback controls (A / B / X / Back / Start)  
   - Live-drive motors from left stick when _not_ in playback  
   - Refresh the LCD status every 200 ms  

3. **Recording Logic**  
   - **LB** toggles recording on/off  
   - **RB** ends the current micro-segment and stores:  
     - `dir1`, `dir2` (±1 or 0)  
     - `pulses1`, `pulses2` (step counts)  
     - `durationMs` (segment time)  
   - Up to 100 segments in RAM  

4. **Playback Logic**  
   - **A** plays forward, **B** plays reverse  
   - Each segment:  
     1. Apply recorded directions and enable drivers  
     2. Evenly space interleaved pulses to match `durationMs`  
     3. Abort with **X** at any time  
   - Drivers disabled at end of playback  

------

## 3. Button & Stick Mapping

| Control  | Record                  | Playback                  | Live Drive                               |
|:--------:|:------------------------|:--------------------------|:-----------------------------------------|
| LB       | Start / Cancel recording| —                         | —                                        |
| RB       | End micro‐segment       | —                         | —                                        |
| A        | —                       | Play forward              | —                                        |
| B        | —                       | Play reverse              | —                                        |
| X        | —                       | Abort playback            | —                                        |
| Back     | Save segments to flash  | —                         | —                                        |
| Start    | Load segments from flash| —                         | —                                        |
| Left Stick (Y/Y+X) | —            | —                         | Forward/back + steering (isolate motor) |

- **Live-drive**:  
  - Push stick up/down for both motors forward/back  
  - Push stick left/right to turn (one side stops)  

---

## 4. Key Functions

### 4.1 Low-Level Motor Control

- **`doStep1()` / `doStep2()`**  
  Generate one step pulse (200 µs HIGH + 200 µs LOW) and increment counters.

- **`enable1(bool)` / `enable2(bool)`**  
  Drive the ENA– pin (LOW = enabled, HIGH = disabled).

### 4.2 Recording Segments

- **`startSegment(dir1, dir2)`**  
  Capture start time and step counters.

- **`recordSegment(dir1, dir2)`**  
  Compute `pulses1`, `pulses2`, `durationMs` and append to array.

### 4.3 Flash Storage

- **`saveToFlash()`** / **`loadFromFlash()`**  
  Persist or retrieve `segmentCount` and segment data in non-volatile storage.

### 4.4 Playback

- **`playbackSequence(reverse)`**  
  Iterate through segments (forward or reverse), apply directions, and
  space pulses evenly over each segment’s duration. Abortable via **X**.

### 4.5 Display

- **`updateDisplay()`**  
  Show recording/playback state and segment count on the LCD.

---

## 5. How to Use

1. Power on M5Stack ⇒ “Hold Xbox bind” appears.  
2. Pair the Xbox controller (hold its bind button).  
3. **Live-drive**: verify motors spin with the left stick.  
4. **Record a path**:  
   - Press **LB** to start recording  
   - Drive with stick; press **RB** to mark segments  
   - Press **LB** again to stop recording  
5. **Save** with **Back**, **Load** with **Start**.  
6. **Playback** with **A** (forward) or **B** (reverse).  
7. **Abort** playback at any time with **X**.  

---

## 6. Next Enhancements

- Display a progress bar or segment index during playback  
- Add “clear all” command to wipe saved segments  
- Scale replay speed by adjusting `durationMs`  
- Improve error handling for full segment buffer  
- Provide on-screen prompts for segment counts and errors  

---
