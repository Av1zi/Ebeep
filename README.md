# Ebeep

![Platform](https://img.shields.io/badge/platform-ESP32--C6-blue)
![Framework](https://img.shields.io/badge/framework-Arduino%20%2F%20PlatformIO-orange)
![License](https://img.shields.io/badge/license-GPL--3.0-green)

Two handheld wireless devices that can message each other and play games, built as a personal gift.

> **E** for eInk. **Beep** for beeper

---

## Project Showcase

### 3D render

https://github.com/user-attachments/assets/e5b205ba-0493-49c5-be56-bcfee51162cf


### Walkthrough
https://github.com/user-attachments/assets/030f00d7-5b2a-4592-a6ae-74eca7de7010

---

## Features

- **Messaging** - 3-button scrollable alphabet, double-space to send
- **Games** - TicTacToe, Connect 4, and Blackjack
- **E-Ink display** - 2.9" screen
- **Battery powered** - USB-C charging
- **Deep sleep** - sleeps when you're not using it to save battery
- **Works anywhere with WiFi** - the two devices talk over MQTT, so they don't need to be near each other

---

## Hardware

| Component | Part |
|---|---|
| Microcontroller | Seeed XIAO ESP32-C6 |
| Display | WeAct 2.9" eInk |
| Battery | 3.7V LiPo, 3000mAh |
| Buttons | 3× low profile mechanical keyboard switches |
| Case | ~76g PLA, 4× M4 heated inserts, 4× M4×25 screws |
| Other | 2× 200kΩ resistors, 5×7cm perfboard |

---

## How It Works

### Communication
Both devices connect to WiFi and talk through an MQTT broker (I used the free HiveMQ Cloud tier). Each device listens on its own topic and publishes to the other one's.

### WiFi Setup
When the device wakes up and none of its known networks are in range, it opens a hotspot (`Ebeep_1_config` or `Ebeep_2_config`). Connect to it from any phone/pc and a setup page pops up automatically, there you will have to enter the wifi details and press done. (to save battery the hotspot will closes by itself after 5 minutes) The device remembers up to 5 networks and picks whichever one is in range.

Walkthrough:

https://github.com/user-attachments/assets/75ae72dc-5833-4c83-8c6c-1c9a049145ae

### Typing Interface
- **Left / Right** - scroll through characters (`SPC A B C ... Z . ! ? <3`)
- **Select** - confirm the current character
- **Two spaces in a row** - send the message
- **DEL on empty** - exit compose

### Games
- **TicTacToe** - classic 3×3. Whoever starts the match first goes first.
- **Connect 4** - 7×6 grid, pieces drop with gravity.
- **Blackjack** - single player (the only one that doesn't need the other device). Dealer stands on soft 17.

### Power Management
The beeper goes to sleep if you don't use it for 10 minutes (you can change this in `config.h`). While asleep it disconnects from WiFi and the MQTT server and basically shuts off to save battery.

Wake it by pressing the left or middle button. On wake-up it looks for a network it remembers, and if it can't find one it starts its own hotspot so you can set up WiFi again (see WiFi Setup).

---

## PCB

A custom PCB has been fully designed and routed in KiCad to replace the hand-wired perfboard build. It's not ordered or fabricated yet, so the build instructions below are still perfboard based. The PCB is a planned upgrade for a future revision.

> [!WARNING]
> The PCB has not been fitted or tested against the case files in this repo. If you order it, expect to redesign the case (or at least the mounting points) to fit. It is **not** a drop-in replacement yet.

<img width="800" height="646" alt="PCB layout" src="https://github.com/user-attachments/assets/c7d6436f-926f-4e3e-8f55-151299118217" />

---

## Building Your Own

### Print Settings
I used the printer default settings, with tree supports. I have a multi-nozzle printer so I used a support interface layer for a cleaner surface, but it's not necessary.


### Electrical Diagram
<img width="2400" height="2400" alt="Ebeep wiring diagram" src="https://github.com/user-attachments/assets/5c826736-3398-4f3e-a769-e0afdf25947a" />

### Building Instructions
1. 3D print the files from the `CAD` folder
2. Solder everything onto the perfboard following the electrical diagram above
3. Flash the code (see **Setting Up the Code** below) and test before closing the case
4. Fit the components into the case and put double-sided tape under the battery to hold it
5. Press in the 4× M4 heated inserts, then screw the case shut with the M4 screws

### Setting Up the Code
1. In `include/config.h` change `DEVICE_NUM` to `1` on one device and `2` on the other
2. Copy `include/secret.example.h` to `include/secret.h` and fill in your MQTT broker credentials (`secret.h` is gitignored so it stays private)
3. Upload via PlatformIO. The `huge_app.csv` partition scheme is required
4. On first boot, connect to the config hotspot (`Ebeep_1_config` or `Ebeep_2_config`) and enter your WiFi credentials

---

## License

GPL 3.0 - Do whatever you want with it, just keep it open source as well.
