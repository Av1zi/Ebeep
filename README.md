
# What is Ebeep?

Ebeep is a DIY electronics project: two standalone devices that can talk to eachother over wifi and play games :)

---

## Features

-  **Send & receive messages** — type freehand using a 3-button scrollable alphabet interface
-  **Buzzer notification** — plays a melody when a new message arrives
-  **Battery powered** — LiPo battery with charging over USB-C, battery % always visible on screen
-  **Works across countries** — communicates over WiFi via MQTT (HiveMQ Cloud free tier)
-  **E-ink display** — for always on display and for ultra low power draw
-  **Expandable** — a Games slot is reserved in the UI for future additions

---

## Hardware

| Component | Part |
|---|---|
| Microcontroller | xiao ESP32C6 |
| Display | Waveshare e-ink (2.9") |
| Battery | 3.7V LiPo, 3000mAh & 200K resistors |
| Buttons | 3× low profile mechanical keyboard switches |

---

## How It Works?

### Communication
Both devices connect to WiFi and talk to a private **MQTT broker**. Each device subscribes to its own topic and publishes to the other's.

### WiFi Setup (captive portal)
On first boot, the device creates a hotspot called `Ebeep-Setup`. Connect to it from any phone and a setup page appears automatically pick your WiFi network, enter the password, done. Credentials are saved to flash and the device never asks again unless its unable to connect to the wifi.

### Typing Interface
Messages are typed using a 3-button scrollable alphabet:
- **Left / Right** - scroll through characters (`_ A B C D E F G H I ...`)
- **Confirm** - select the current character
- **Two spaces in a row** - sends the message
- **Del** - deletes a character \ exits the screen if there is no characters to delete

---

## Why "Ebeep"?

**E** for e-ink. **Beep** for beeper becuse this device is inspired by a beeper.

---

## License
  do whatever you want with it, just make sure its open source as well.
