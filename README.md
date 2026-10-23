# Ebeep

![Platform](https://img.shields.io/badge/platform-ESP32--C6-blue)
![Framework](https://img.shields.io/badge/framework-Arduino%20%2F%20PlatformIO-orange)
![License](https://img.shields.io/badge/license-GPL--3.0-green)

Two handheld wireless devices that can message each other and play games, built as a personal gift.

> **E** for eInk. **Beep** for beeper

---

## Project Showcase

### 3d render
https://github.com/user-attachments/assets/a6d2f698-0696-4dca-adf1-06d3810ce39c

### walkthrough
https://github.com/user-attachments/assets/030f00d7-5b2a-4592-a6ae-74eca7de7010

---

## Features

- **Messaging** - 3-button scrollable alphabet, double-space to send
- **Games** - TicTacToe, Connect 4, and Blackjack, all multiplayer over MQTT
- **E-Ink display** - 2.9" screen and low power consumption
- **Battery powered** - LiPo with USB-C charging, percentage always visible
- **Deep sleep** - sleeps until you press a button, so a sleeping device uses almost nothing
- **Long range** - talks over WiFi/MQTT, so both devices work from anywhere with an internet connection

---

## Hardware

| Component | Part |
|---|---|
| Microcontroller | Seeed XIAO ESP32-C6 |
| Display | WeAct 2.9" eInk |
| Battery | 3.7V LiPo, 3000mAh |
| Buttons | 3× low profile mechanical keyboard switches |
| Case | ~76g PLA filament <br> 4× M4 heated inserts <br> 4× M4×25 screws |
| Other | 2× 200kΩ resistors (voltage divider for battery sensing) <br> 5×7cm perfboard |

---

## How It Works

### Communication
Both devices connect to WiFi and talk through a private MQTT broker. Each device subscribes to its own topic and publishes to the other's.

### WiFi Setup
When the device wakes up and none of its known networks are in range, it opens a hotspot (`Ebeep_1_config` or `Ebeep_2_config`) and says so on screen. Connect to it from any phone and a setup page pops up automatically - pick your network, enter the password, done. Press the middle button to cancel; the hotspot also closes by itself after 5 minutes. The device remembers up to 5 networks (home, work, phone hotspot...) and picks whichever one is in range, so each place only needs setting up once. see walkthrough below:

https://github.com/user-attachments/assets/75ae72dc-5833-4c83-8c6c-1c9a049145ae



### Typing Interface
- **Left / Right** - scroll through characters (`SPC A B C ... Z . ! ? <3`)
- **Select** - confirm the current character
- **Two spaces in a row** - send the message
- **DEL on empty** - exit compose

### Games

- **TicTacToe** - classic 3×3, first message sent starts the match
- **Connect 4** - 7×6 grid, gravity-based drop
- **Blackjack** - single-player, full ruleset with split, double, and a soft-17 dealer

### Power Management
The device stays in deep sleep with "Sleeping" on the screen and never wakes up on its own. Press the left or middle button to wake it: it reconnects to the last network straight away (no scan), or scans for any other known network, and falls back to the setup hotspot if none are around. It stays awake for 3 minutes after the last button press or message, then goes back to sleep. Messages are sent as retained MQTT messages, so the last message sent while you were asleep is waiting for you when you wake it. Timings are in `config.h`.

---

## PCB

A custom PCB has been fully designed and routed in KiCad to replace the hand-wired perfboard build. It's not ordered/fabricated yet, so the current build instructions below are still perfboard based (the PCB is a planned upgrade for a future revision.)

> [!WARNING]
> The PCB has not been fitted or tested against the STL case files in this repo. If you order it, expect to redesign the case (or at least the mounting points) to fit. it is **not** a drop-in replacement yet.

<img width="800" height="646" alt="image" src="https://github.com/user-attachments/assets/c7d6436f-926f-4e3e-8f55-151299118217" />


---

## Building Your Own

### Electrical Diagram
<img width="2400" height="2400" alt="image" src="https://github.com/user-attachments/assets/5c826736-3398-4f3e-a769-e0afdf25947a" />


### Building Instructions
1. 3D print the files from the `CAD` folder
2. Solder everything onto the perfboard following the electrical diagram above
3. Flash the code (see **Setting Up the Code** below) and test before closing the case
4. Fit the components into the case & add double-sided tape under the battery to secure it
5. Press in the 4× M4 heated inserts, then screw the case shut with the M4 screws

### Setting Up the Code
1. Change `DEVICE_NUM` to `1` on one device and `2` on the other
2. Copy `include/secret.example.h` to `include/secret.h` and fill in your MQTT broker credentials (`secret.h` is gitignored so it stays private)
3. Upload via PlatformIO - the `huge_app.csv` partition scheme is required
4. On first boot, connect to the config hotspot (`Ebeep_1_config` or `Ebeep_2_config`) and enter your WiFi credentials

---

## License

GPL 3.0 - Do whatever you want with it, just keep it open source as well.
