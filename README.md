# Fencing-Scoring-Machine


An open-source **Arduino-based fencing scoring system** that detects touches and manages time, score, cards, and more. Touch register comes from the All-Weapon-Box open source code at https://github.com/wnew/fencing_scoring_box/blob/master/firmware/allweaponbox/allweaponbox.ino. Display code is all original.

---

## Project Overview

This project implements a complete fencing scoring machine built from low-cost, off-the-shelf components and open-source code.  
It is divided into **two main parts**:

1. **Hardware** — the circuit design, wiring diagrams, and power layout.  
2. **Firmware** — the code that drives the scoring logic, lights, sounds, and timers.

The system is compatible with standard fencing weapons and scoring rules (épée, foil, sabre) and uses Arduino microcontrollers for control and logic.

---

##  Repository Structure

```
fencing-scoring-machine/
├── hardware/
│   ├── schematics/
│   ├── pcb-layouts/
│   └── README.md
│
└── firmware/
    ├── touch-handler/
    │   └── three_weapon_box/
    │       └── three_weapon_box.ino
    ├── touch-matrix-handler/
    │   └── touch_matrix_handler/
    │        └── touch_matrix_handler.ino
    └── display-handler/
        └── display_handler/
            └── display_handler.ino
```

### **hardware/**
Contains all the circuit schematics, wiring diagrams, and mechanical drawings required to assemble the scoring box.  
Includes documentation for the LED matrix, buzzer, IR receiver, and weapon signal inputs.

### **firmware/**
Contains the Arduino sketches and supporting code for the two main control modules:

- **`touch-handler/`**  
  Handles weapon signal detection and touch registration.  
  Based on the open-source [`three_weapon_box`](https://github.com/dkroeske/three_weapon_box) project, adapted for Arduino Uno hardware.

- **`display-handler/`**  
  Manages match time, score, penalty cards, and light/sound indicators.  
  Entirely original code, written for the Arduino Mega to take advantage of additional I/O pins and memory.

- **`touch-matrix-handler/`**  
  Firmware for lighting up colorduino-type matrices to display touch and off-target lights.

---

## ⚙️ Hardware Summary

| Component | Description | MCU |
|------------|--------------|-----|
| **Touch Handler** | Detects valid touches and differentiates between weapons. | Arduino **Uno** |
| **Display Handler** | Drives LED matrices, buzzer, indicators, and manages logic. | Arduino **Mega** |
| **Touch Matrix Handler** | Internal logic for the colorduino touch lights. | Colorduino Matrix (8x8) |
| **Power** | Shared regulated 5 V supply; separate logic and LED lines for stability. | – |

---

## 🚀 Getting Started

### 1. Clone the repository
```bash
git clone https://github.com/<your-username>/fencing-scoring-machine.git
```

### 2. Upload firmware
- Open `firmware/touch-handler/three_weapon_box/three_weapon_box.ino` in the Arduino IDE and upload to **Arduino Uno**.  
- Open `firmware/display-handler/display_handler/display_handler.ino` and upload to **Arduino Mega**.
- Open `firmware/touch-matrix-handler/touch_matrix_handler/touch-matrix-handler.ino` and upload to both colorduino touch lights (requires FTDI adapter).

### 3. Assemble hardware
Use the schematics in the `hardware/` directory to wire the modules.  
Ensure both microcontrollers share a common ground when interconnected.

### 4. Power up
When powered, the system initializes the display, waits for valid weapon connections, and begins match timing once both sides are active.

---

## 🔧 Dependencies

- [Arduino IDE](https://www.arduino.cc/en/software)
- [LedControl library](https://github.com/wayoda/LedControl)
- [three_weapon_box library](https://github.com/dkroeske/three_weapon_box) (included or referenced)
- [Adafruit NeoPixel library](https://github.com/adafruit/Adafruit_NeoPixel)
- Standard electronic components (MAX7219, LEDs, buzzers, etc.)

---

## 📜 License

This project is open-source and released under the **MIT License**.  
The `touch-handler` code includes or references portions from the [`three_weapon_box`](https://github.com/dkroeske/three_weapon_box) project, which is licensed under GPLv3.

---

## 🙌 Acknowledgments

- **three_weapon_box** — for some of the foundational touch detection logic.  