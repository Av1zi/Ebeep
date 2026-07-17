# Ebeep

Two handheld wireless devices that can message each other and play games, built as a personal gift.

> **E** for eInk. **Beep** for beeper

---

## Showcase

https://github.com/user-attachments/assets/030f00d7-5b2a-4592-a6ae-74eca7de7010

---

## Features

- **Messaging** - 3-button scrollable alphabet, double-space to send
- **Games** - TicTacToe, Connect 4, and Blackjack (multiplayer over MQTT)
- **E-Ink display** - always-on, ultra low power, 2.9"
- **Battery powered** - LiPo with USB-C charging, percentage always visible
- **Deep sleep** - wakes on timer or button press to maximise battery life
- **Works across countries** - communicates over WiFi via MQTT

---

## Hardware

| Component | Part |
|---|---|
| Microcontroller | Seeed XIAO ESP32-C6 |
| Display | WeAct 2.9" eInk |
| Battery | 3.7V LiPo, 3000mAh |
| Buttons | 3× low profile mechanical keyboard switches |
| Case | * 76~ grams of PLA filament <br> * 4× M4 heated inserts <br> * 4× M4x25 screws   |
| Other |  * 2× 200kΩ resistors (voltage divider for battery sensing)<br> * 5x7cm PrefBoard |

---

## How It Works

### Communication
Both devices connect to WiFi and talk through a private MQTT broker. Each device subscribes to its own topic and publishes to the other's. Messages and game moves are sent in real time.

### WiFi Setup
On first boot the device creates a hotspot (`Ebeep_1_config` or `Ebeep_2_config`). Connect from any phone a setup page appears automatically. Pick your network, enter the password, done. Credentials are saved to flash and never asked for again unless the connection fails.

### Typing Interface
- **Left / Right** - scroll through characters (`SPC A B C ... Z . ! ? <3`)
- **Select** - confirm the current character
- **Two spaces in a row** - send the message
- **DEL on empty** - exit compose

### Games
All multiplayer games use a handshake over MQTT to sync state — no server-side logic, just the two devices talking directly.

- **TicTacToe** - classic 3×3, first to connect wins
- **Connect 4** - 7×6 grid, gravity-based drop
- **Blackjack** - singlePlayer, full ruleset with split, double, soft 17 dealer...

### Power Management
The device spends most of its time in deep sleep, waking every 30 seconds to check for messages. Night hours (00:00–08:00) and low battery stretch that to 2 minutes. Any button press or incoming message triggers a 10-minute active window before going back to sleep.

---

## Building Your Own

### Electrical Diagram
<img width="2400" height="2400" alt="Ebeep wiring diagram" src="https://github.com/user-attachments/assets/2caf1f5a-6e71-49ab-91ef-3e4e569b0644" />

### Building Instructions
1. 3D print the files from the `STL` folder
2. Solder everything onto the perfboard following the electrical diagram above
3. Flash the code (see **Setting Up the Code** below) and test before closing the case
4. Fit the components into the case & add double-sided tape under the battery to secure it
5. Press in the 4× M4 heated inserts, then screw the case shut with the M4×25 screws

### Setting Up the Code
1. Change `DEVICE_NUM` to `1` on one device and `2` on the other
2. Create `secret.h` with your MQTT broker credentials (excluded from the repo via `.gitignore`)
3. Upload via PlatformIO, the `huge_app.csv` partition scheme is required
4. On first boot, connect to the config hotspot (`Ebeep_1_config` or `Ebeep_2_config`) and enter your WiFi credentials

---

## License

GPL 3.0 - Do whatever you want with it, just keep it open source as well.
